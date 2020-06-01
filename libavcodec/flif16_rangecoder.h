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

#include "libavutil/mem.h"
#include "libavutil/intmath.h"
#include "bytestream.h"

#include <stdio.h> // Remove
#include <stdint.h>
#include <assert.h>

#define FLIF16_RAC_MAX_RANGE_BITS 24
#define FLIF16_RAC_MIN_RANGE_BITS 16
#define FLIF16_RAC_MAX_RANGE (uint32_t) 1 << FLIF16_RAC_MAX_RANGE_BITS
#define FLIF16_RAC_MIN_RANGE (uint32_t) 1 << FLIF16_RAC_MIN_RANGE_BITS


typedef struct FLIF16ChanceTable {
    uint16_t zero_state[4096];
    uint16_t one_state[4096];
} FLIF16ChanceTable;

typedef struct FLIF16Log4kTable {
    uint16_t table[4096];
    int scale;
} FLIF16Log4kTable;

typedef struct FLIF16RangeCoder {
    uint32_t range;
    uint32_t low;
    uint16_t chance;
    FLIF16ChanceTable *ct;
    FLIF16Log4kTable *log4k;
    GetByteContext *gb;
} FLIF16RangeCoder;

FLIF16RangeCoder *ff_flif16_rac_init(GetByteContext *gb);
void ff_flif16_chancetable_init(FLIF16RangeCoder *rc, int alpha, int cut);
void ff_flif16_build_log4k_table(FLIF16RangeCoder *rc);

// NearZero Integer Definitions:
// Maybe pad with extra 2048s for faster access like in original code.
static uint16_t flif16_nz_int_chances[20] = {
    1000, // Zero
    2048, // Sign
    
    // Exponents
    1000, 1200, 1500, 1750, 2000, 2300, 2800, 2400, 2300, 
    2048, // <- exp >= 9
    
    // Mantisaa
    1900, 1850, 1800, 1750, 1650, 1600, 1600, 
    2048 // <- mant > 7
};

#define NZ_INT_ZERO  (flif16_nz_int_chances[0])
#define NZ_INT_SIGN  (flif16_nz_int_chances[1])
#define NZ_INT_EXP(k) ((k < 9) ? flif16_nz_int_chances[2 + (k)] : \
                                 flif16_nz_int_chances[11])
#define NZ_INT_MANT(k) ((k < 8) ? flif16_nz_int_chances[12 + (k)] : \
                                  flif16_nz_int_chances[19])

// Functions

static inline void ff_flif16_rac_renorm(FLIF16RangeCoder *rc)
{
    while (rc->range <= FLIF16_RAC_MIN_RANGE) {
        rc->low <<= 8;
        rc->range <<= 8;
        rc->low |= bytestream2_get_byte(rc->gb);
    }
}

static inline uint8_t ff_flif16_rac_get(FLIF16RangeCoder *rc, uint32_t chance)
{
    // assert(rc->chance > 0);
    // assert(rc->chance < rc->range);

    // printf("[%s] low: %u range: %u chance: %u\n", __func__, rc->low, rc->range, chance);
    if (rc->low >= rc->range - chance) {
        rc->low -= rc->range - chance;
        rc->range = chance;
        ff_flif16_rac_renorm(rc);
        return 1;
    } else {
        rc->range -= chance;
        ff_flif16_rac_renorm(rc);
        return 0;
    }
}

static inline uint8_t ff_flif16_rac_read_bit(FLIF16RangeCoder *rc)
{
    return ff_flif16_rac_get(rc, rc->range >> 1);
}

static inline uint32_t ff_flif16_rac_read_chance(FLIF16RangeCoder *rc,
                                                 uint16_t b12)
{
    // assert((b12 > 0) && (b12 >> 12) == 0);
    uint32_t ret;
    // Optimisation based on CPU bus size (32/64 bit)
    if (sizeof(rc->range) > 4) 
        ret = ((rc->range) * b12 + 0x800) >> 12;
    else 
        ret = ((((rc->range) & 0xFFF) * b12 + 0x800) >> 12) + 
              (((rc->range) >> 12) * b12);
    
    return ff_flif16_rac_get(rc, ret);
}

/**
 * Reads a Uniform Symbol Coded Integer.
 */
static inline int ff_flif16_rac_read_uni_int(FLIF16RangeCoder *rc, 
                                             int min, int len)
{
    // assert(len >= 0);
    int med;
    uint8_t bit;
    
    while (len > 0) {
        bit = ff_flif16_rac_read_bit(rc);
        med = len / 2;
        if (bit) {
            min = min + med + 1;
            len = len - (med + 1);
        } else {
            len = med;
        }
       //__PLN__
    }
    return min;
}

// Overload of above function in original code

static inline int ff_flif16_rac_read_uni_int_bits(FLIF16RangeCoder *rc,
                                                  int bits)
{
    return ff_flif16_rac_read_uni_int(rc, 0, (1 << bits) - 1);
}

// Nearzero integer definitions

void inline ff_flif16_chancetable_put(FLIF16RangeCoder *rc, 
                                      uint8_t bit, uint16_t chance)
{
    rc->chance = (!bit) ? rc->ct->zero_state[chance]
                        : rc->ct->one_state[chance];
}

/**
 * Reads a near-zero encoded symbol into the RAC probability model/chance table;
 * @param chance The symbolchance specified by the NZ_INT_* macros
 */
static inline uint8_t ff_flif16_rac_read_symbol(FLIF16RangeCoder *rc,
                                                uint16_t chance)
{
    uint8_t bit = ff_flif16_rac_read_chance(rc, chance);
    ff_flif16_chancetable_put(rc, bit, chance);
    return bit;
}

/**
 * Returns an integer encoded by zear zero integer encoding.
 */
static inline int ff_flif16_rac_read_nz_int(FLIF16RangeCoder *rc, int min,
                                            int max)
{
    // assert(min<=max);
    uint8_t sign;
    const int amin = 1;
    const int amax = (sign ? max : -min);
    
    const int emax = ff_log2(amax);
    int e          = ff_log2(amin);
    int have, left, minabs1, maxabs0;

    if (min == max)
        return min;

    // assert(min <= 0 && max >= 0); // should always be the case, because guess should always be in valid range

    if (ff_flif16_rac_read_symbol(rc, NZ_INT_ZERO))
        return 0;

    if (min < 0) {
        if (max > 0)
            sign = ff_flif16_rac_read_symbol(rc, NZ_INT_SIGN);
        else
            sign = 0;
    } else
        sign = 1;

    for (; e < emax; e++) {
        // if exponent >e is impossible, we are done
        // actually that cannot happen
        //if ((1 << (e+1)) > amax) break;
        if (ff_flif16_rac_read_symbol(rc, NZ_INT_EXP((e << 1) + sign)))
            break;
    }

    have = (1 << e);
    left = have-1;
    for (int pos = e; pos>0;) {
        left >>= 1;
        pos--;
        minabs1 = have | (1<<pos);
        maxabs0 = have | left;
        if (minabs1 > amax) {
            continue;
        } else if (maxabs0 >= amin) { // 0-bit and 1-bit are both possible
            if (ff_flif16_rac_read_symbol(rc, NZ_INT_MANT(pos)))
                have = minabs1;
        } 
        else
            have = minabs1;
    }

    return (sign ? have : -have);
}

static inline int ff_flif16_rac_read_gnz_int(FLIF16RangeCoder *rc, int min,
                                             int max)
{
    if (min > 0) 
        return ff_flif16_rac_read_nz_int(rc, 0, max - min) + min;
    else if (max < 0) 
        return ff_flif16_rac_read_nz_int(rc, min - max, 0) + max;
    else 
        return ff_flif16_rac_read_nz_int(rc, min, max);
}

#endif /* FLIF16_RANGECODER_H */


