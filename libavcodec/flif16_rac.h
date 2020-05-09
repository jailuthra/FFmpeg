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

#include "libavutil/avassert.h"
#include "libavutil/mem.h"
#include "bytestream.h"

#include <stdio.h> // Remove
#include <stdint.h>
#include <assert.h>

#define FLIF16_RAC_MAX_RANGE_BITS 24
#define FLIF16_RAC_MIN_RANGE_BITS 16
#define FLIF16_RAC_MAX_RANGE (uint32_t) 1 << FLIF16_RAC_MAX_RANGE_BITS
#define FLIF16_RAC_MIN_RANGE (uint32_t) 1 << FLIF16_RAC_MIN_RANGE_BITS


typedef struct FLIF16RangeCoder {
    uint32_t range;
    uint32_t low;
    GetByteContext *gb;
} FLIF16RangeCoder;

FLIF16RangeCoder *ff_flif16_rac_init();

static inline uint32_t ff_flif16_rac_read_chance(uint16_t b12, uint32_t range)
{
    // assert((b12 > 0) && (b12 >> 12) == 0);

    // Optimisation based on CPU bus size (32/64 bit)
    if (sizeof(range) > 4) 
        return (range * b12 + 0x800) >> 12;
    else 
        return ((((range & 0xFFF) * b12 + 0x800) >> 12) + 
               ((range >> 12) * b12));
}

/*
bool inline read_12bit_chance(uint16_t b12) ATTRIBUTE_HOT
{
    return get(Config::chance_12bit_chance(b12, range));
}
*/

inline void ff_flif16_rac_renorm(FLIF16RangeCoder *rc)
{
    // Replaced with a while loop
    while (range <= FF_FLIF16_MIN_RANGE) {
        rc->low <<= 8;
        rc->range <<= 8;
        rc->low |= ff_flif16_rac_read(rc);
    }
}

inline uint8_t ff_flif16_rac_get(FLIF16RangeCoder *rc, uint32_t chance)
{
    // assert(rc->chance > 0);
    // assert(rc->chance < rc->range);
    if (rc->low >= rc->range - rc->chance) {
        rc->low -= rc->range - rc->chance;
        rc->range = rc->chance;
        ff_flif16_rac_renorm(rc);
        return 1;
    } else {
        rc->range -= rc->chance;
        ff_flif16_rac_renorm(rc);
        return 0;
    }
}

uint8_t inline ff_flif16_rac_read_bit(FLIF16RangeCoder *rc)
{
    return ff_flif16_rac_get(rc, rc->range >> 1);
}

/**
 * Reads a Uniform Symbol Coded Integer.
*/
inline int ff_flif16_rac_read_uni_int(FLIF16RangeCoder *rc, int min, int len)
{
    // assert(len >= 0);
    int med;
    uint8_t bit;
    
    while (len > 0) {
        bit = ff_flif16_rac_read_bit(rc);
        med = len / 2;
        if (bit) {
            min = min + med + 1;
            len = len - (med + 1));
        }
    }
    return min;
}

/* overload of above function
int read_int(int bits)
{
    return read_int(0, (1<<bits)-1);
}
*/


#endif /* FLIF16_RANGECODER_H */


