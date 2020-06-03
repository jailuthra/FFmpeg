/*
 * Range coder for FLIF16
 * Copyright (c) 2020 Anamitra Ghorui
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * Range coder for FLIF16.
 */

#ifndef FLIF16_RANGECODER_H
#define FLIF16_RANGECODER_H

#include "rangecoder.h"
#include "flif16.h"

#include "libavutil/mem.h"
#include "libavutil/intmath.h"
#include "bytestream.h"

#include <stdio.h> // Remove
#include <stdint.h>
#include <assert.h>

#define FLIF16_RAC_MAX_RANGE_BITS 24
#define FLIF16_RAC_MAX_RANGE_BYTES (FLIF16_RAC_MAX_RANGE_BITS / 8)
#define FLIF16_RAC_MIN_RANGE_BITS 16
#define FLIF16_RAC_MAX_RANGE (uint32_t) 1 << FLIF16_RAC_MAX_RANGE_BITS
#define FLIF16_RAC_MIN_RANGE (uint32_t) 1 << FLIF16_RAC_MIN_RANGE_BITS


typedef enum FLIF16RACTypes {
    FLIF16_RAC_BIT = 0,
    FLIF16_RAC_UNI_INT,
    FLIF16_RAC_CHANCE,
    FLIF16_RAC_NZ_INT,
    FLIF16_RAC_GNZ_INT
} FLIF16RACReaders;

typedef struct FLIF16ChanceTable {
    uint16_t zero_state[4096];
    uint16_t one_state[4096];
} FLIF16ChanceTable;

typedef struct FLIF16Log4kTable {
    uint16_t table[4096];
    int scale;
} FLIF16Log4kTable;

// rc->renorm may be useless. Check.

typedef struct FLIF16RangeCoder {
    unsigned int range;
    unsigned int low;
    uint16_t chance;
    uint8_t empty;    ///< Is bytestream empty
    uint8_t renorm;   ///< Is a renormalisation required 
    uint8_t active;   ///< Is an integer reader currently active (to save/
                      ///  transfer state)
    
    // uni_int state management
    uint32_t min;
    uint32_t len;
    
    // nz_int state management
    uint8_t segment; ///< The "segment" the function currently is in
    uint8_t  sign;
    int amin, amax, emax, e, have, left, minabs1, maxabs0, pos;

    // There needs to be a chance context.
    // nz_int uses different individual chances for each 0, sign, exp and mant
    FLIF16ChanceTable *ct;
    FLIF16Log4kTable *log4k;
    GetByteContext *gb;
} FLIF16RangeCoder;

// NearZero Integer Definitions:
extern uint16_t flif16_nz_int_chances[20];

#define NZ_INT_ZERO (0)
#define NZ_INT_SIGN (1)
#define NZ_INT_EXP(k) ((k < 9) ? (2 + (k)) : 11)
#define NZ_INT_MANT(k) ((k < 8) ? (12 + (k)) : 19)


// Maybe rename to symbol context
typedef uint16_t FLIF16ChanceContext;

FLIF16RangeCoder *ff_flif16_rac_init(GetByteContext *gb, uint8_t *buf,
                                     uint8_t buf_size);
void ff_flif16_rac_free(FLIF16RangeCoder *rc);
void ff_flif16_chancetable_init(FLIF16RangeCoder *rc, int alpha, int cut);
void ff_flif16_build_log4k_table(FLIF16RangeCoder *rc);
FLIF16ChanceContext *ff_flif16_chancecontext_init(void);

// Functions

static inline int ff_flif16_rac_renorm(FLIF16RangeCoder *rc)
{
    uint32_t left = bytestream2_get_bytes_left(rc->gb);
    printf("[%s] left = %d\n", __func__, left);
    if (!left)
        return 0;
    while (rc->range <= FLIF16_RAC_MIN_RANGE) {
        rc->low <<= 8;
        rc->range <<= 8;
        rc->low |= bytestream2_get_byte(rc->gb);
        if(!left)
            return 0;
        else
            --left;
    }
    rc->renorm = 0;
    return 1;
}

static inline uint8_t ff_flif16_rac_get(FLIF16RangeCoder *rc, uint32_t chance,
                                        uint8_t *target)
{
    // assert(rc->chance > 0);
    // assert(rc->chance < rc->range);

    // printf("[%s] low: %u range: %u chance: %u\n", __func__, rc->low, rc->range, chance);
    if (rc->renorm) {
        printf("[%s] Triggered\n", __func__);
        return 0;
    }

    if (rc->low >= rc->range - chance) {
        rc->low -= rc->range - chance;
        rc->range = chance;
        *target = 1;
    } else {
        rc->range -= chance;
        *target = 0;
    }
    
    rc->renorm = 1;
    
    return 1;
}

static inline uint8_t ff_flif16_rac_read_bit(FLIF16RangeCoder *rc,
                                             uint8_t *target)
{
    return ff_flif16_rac_get(rc, rc->range >> 1, target);
}

static inline uint32_t ff_flif16_rac_read_chance(FLIF16RangeCoder *rc,
                                                 uint16_t b12, uint8_t *target)
{
    uint32_t ret;
    
    if (rc->renorm) {
        printf("[%s] Triggered\n", __func__);
        return 0;
    }
    // assert((b12 > 0) && (b12 >> 12) == 0);
    // Optimisation based on CPU bus size (32/64 bit)
    if (sizeof(rc->range) > 4) 
        ret = ((rc->range) * b12 + 0x800) >> 12;
    else 
        ret = (((((rc->range) & 0xFFF) * b12 + 0x800) >> 12) + 
              (((rc->range) >> 12) * b12));
    
    return ff_flif16_rac_get(rc, ret, target);
}

/**
 * Reads a Uniform Symbol Coded Integer.
 */
static inline int ff_flif16_rac_read_uni_int(FLIF16RangeCoder *rc, 
                                             uint32_t min, uint32_t len,
                                             uint32_t *target)
{
    // assert(len >= 0);
    int med;
    uint8_t bit;

    if (rc->renorm) {
        printf("[%s] Triggered\n", __func__);
        return 0;
    }

    if (!rc->active) {
        rc->min = min;
        rc->len = len;
        rc->active = 1;
    }
    
    if ((rc->len) > 0) {
        ff_flif16_rac_read_bit(rc, &bit);
        med = (rc->len) / 2;
        if (bit) {
            rc->min += med + 1;
            rc->len -= med + 1;
        } else {
            rc->len = med;
        }
        //__PLN__
        printf("[%s] min = %d , len = %d\n", __func__, rc->min, rc->len);
        return 0;
    } else {
        *target = rc->min;
        printf("[%s] target = %d\n", __func__, rc->min);
        rc->active = 0;
        return 1;
    }
}

// Overload of above function in original code

static inline int ff_flif16_rac_read_uni_int_bits(FLIF16RangeCoder *rc,
                                                  int bits, uint32_t *target)
{
    return ff_flif16_rac_read_uni_int(rc, 0, (1 << bits) - 1, target);
}

// Nearzero integer definitions

static inline void ff_flif16_chancetable_put(FLIF16RangeCoder *rc, 
                                             FLIF16ChanceContext *ctx,
                                             uint16_t type, uint8_t bit)
{
    ctx[type] = (!bit) ? rc->ct->zero_state[ctx[type]]
                       : rc->ct->one_state[ctx[type]];
}

/**
 * Reads a near-zero encoded symbol into the RAC probability model/chance table;
 * @param chance The symbol chance specified by the NZ_INT_* macros
 */

static inline uint8_t ff_flif16_rac_read_symbol(FLIF16RangeCoder *rc,
                                                FLIF16ChanceContext *ctx,
                                                uint16_t type, 
                                                uint8_t *target)
{
    ff_flif16_rac_read_chance(rc, ctx[type], target);
    ff_flif16_chancetable_put(rc, ctx, type, *target);
    return 1;
}

/*
    bool read(SymbolChanceBitType typ, int i = 0)
    {
        BitChance& bch = ctx.bit(typ, i);
        bool bit = rac.read_12bit_chance(bch.get_12bit());
        bch.put(bit, table);
        return bit;
    }
    uint16_t inline get_12bit() const
    {
        return chance;
    }
    void set_12bit(uint16_t chance)
    {
        this->chance = chance;
    }
    void inline put(bool bit, const Table &table)
    {
        chance = table.next[bit][chance];
    }
 */

static inline int ff_flif16_rac_nz_read_internal(FLIF16RangeCoder *rc,
                                                 FLIF16ChanceContext *ctx,
                                                 uint16_t type, uint8_t *target)
{
    int flag = 0;
    // Maybe remove the while loop
    while (!flag) {
        printf("[%s] low = %d range = %d renorm = %d\n", __func__, rc->low, 
               rc->range, rc->renorm);
        if (rc->renorm) {
            if(!ff_flif16_rac_renorm(rc))
                return 0; // EAGAIN condition
        }
        flag = ff_flif16_rac_read_symbol(rc, ctx, type, target);
    }
    return 1;
}

#define RAC_NZ_GET(rc, ctx, chance, target) \
    if (!ff_flif16_rac_nz_read_internal((rc), (ctx), (chance), (uint8_t *) (target))) {\
        goto need_more_data; \
    }

/**
 * Returns an integer encoded by near zero integer encoding.
 */
static inline int ff_flif16_rac_read_nz_int(FLIF16RangeCoder *rc,
                                            FLIF16ChanceContext *ctx,
                                            int min, int max, int *target)
{
    int temp;
    
    if (min == max) {
        *target = min;
        goto end;
    }

    if (!rc->active) {
        rc->segment = 0;
        rc->amin    = 1;
        rc->active  = 1;
    }

    switch (rc->segment) {
        case 0:
            // ff_flif16_rac_read_symbol(rc, NZ_INT_ZERO, &temp);
            RAC_NZ_GET(rc, ctx, NZ_INT_SIGN, &(temp));
            if (temp) {
                *target = 0;
                goto end;
            }
            ++rc->segment;
        
        case 1:
            if (min < 0) {
                if (max > 0) {
                    // ff_flif16_rac_read_symbol(rc, NZ_INT_SIGN, &(rc->sign));
                    RAC_NZ_GET(rc, ctx, NZ_INT_SIGN, &(rc->sign));
                } else
                    rc->sign = 0;
            } else
                rc->sign = 1;

            rc->amax = (rc->sign ? max : -min);
            rc->emax = ff_log2(rc->amax);
            rc->e    = ff_log2(rc->amin);
            ++rc->segment;

        case 2:
            for (; (rc->e) < (rc->emax); (rc->e++)) {
                /*ff_flif16_rac_read_symbol(rc, NZ_INT_EXP((rc->e << 1) + rc->sign),
                                            &temp);*/
                RAC_NZ_GET(rc, ctx, NZ_INT_EXP(((rc->e) << 1) + rc->sign), &(temp));
                if (temp)
                    break;
            }
            rc->have = (1 << (rc->e));
            rc->left = rc->have - 1;
            rc->pos  = rc->e;
            ++rc->segment;

        /* 
         * case 3 and case 4 mimic a for loop.
         * This is done to separate the RAC read statement.
         * for(pos = e; pos > 0; --pos) ...
         */ 
        case 3:
            loop:
            if ((rc->pos) <= 0)
                goto end;
            --(rc->pos);
            rc->left >>= 1;
            rc->minabs1 = (rc->have) | (1 << (rc->pos));
            rc->maxabs0 = (rc->have) | (rc->left);
            ++rc->segment;
        
        case 4:
            if ((rc->minabs1) > (rc->amax)) {
                goto loop;
            } else if ((rc->maxabs0) >= (rc->amin)) {
                // ff_flif16_rac_read_symbol(rc, NZ_INT_MANT(rc->pos), &temp);
                RAC_NZ_GET(rc, ctx, NZ_INT_MANT(rc->pos), &temp)
                if (temp)
                    rc->have = rc->minabs1;
            } 
            else
                rc->have = rc->minabs1;
            --rc->segment;
            goto loop;
    }

    end:
    *target = ((rc->sign) ? (rc->have) : -(rc->have));
    rc->active = 0;
    return 1;
    
    need_more_data:
    return 0;
}


static inline int ff_flif16_rac_read_gnz_int(FLIF16RangeCoder *rc,
                                             FLIF16ChanceContext *ctx,
                                             int min, int max, int *target)
{
    int ret;

    if (min > 0) {
        ret = ff_flif16_rac_read_nz_int(rc, ctx, 0, max - min, target);
        if (ret)
            *target += min;
        
    } else if (max < 0) {
        ret =  ff_flif16_rac_read_nz_int(rc, ctx, min - max, 0, target);
        if (ret)
            *target += max;
    } else
        ret = ff_flif16_rac_read_nz_int(rc, ctx, min, max, target);
    
    return ret;
}

/**
 * Reads an integer encoded by FLIF's RAC.
 * @param val1 A generic value, chosen according to the required type
 * @param val2 Same as val1
 * @param target The place where the resultant value should be written to
 * @param type The type of the integer to be decoded specified by FLIF16RACTypes
 * 
 * @return 0 on bytestream empty, 1 on successful decoding.
 */
static inline int ff_flif16_rac_process(FLIF16RangeCoder *rc,
                                        FLIF16ChanceContext *ctx,
                                        int val1, int val2, void *target, 
                                        int type)
{
    int flag = 0;
    while (!flag) {
        printf("[%s] low = %d range = %d renorm = %d\n", __func__, rc->low, 
               rc->range, rc->renorm);
        if(rc->renorm) {
            if(!ff_flif16_rac_renorm(rc))
                return 0; // EAGAIN condition
        }

        switch (type) {
            case FLIF16_RAC_BIT:
                flag = ff_flif16_rac_read_bit(rc, (uint8_t *) target);
                break;

            case FLIF16_RAC_UNI_INT:
                flag = ff_flif16_rac_read_uni_int(rc, val1, val2, 
                                                  (uint32_t *) target);
                break;
                
            case FLIF16_RAC_CHANCE:
                flag = ff_flif16_rac_read_chance(rc, val1, (uint8_t *) target);
                break;
            
            case FLIF16_RAC_NZ_INT:
                // handle nz_ints
                flag = ff_flif16_rac_read_nz_int(rc, ctx, val1, val2, (int *) target);
                break;
            
            case FLIF16_RAC_GNZ_INT:
                // handle gnz_ints
                flag = ff_flif16_rac_read_gnz_int(rc, ctx, val1, val2, (int *) target);
                break;
            
            default:
                printf("[%s] unknown rac reader\n", __func__);
                break;
        }
    }

    return 1;
}

#define RAC_GET(rc, ctx, val1, val2, target, type) \
    if (!ff_flif16_rac_process((rc), (ctx), (val1), (val2), \
        (void *) (target), (type))) \
        goto need_more_data;

#endif /* FLIF16_RANGECODER_H */
