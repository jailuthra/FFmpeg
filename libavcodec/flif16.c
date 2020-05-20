/*
 * FLIF16 Image Format Utility functions
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
 
#include "flif16.h"

// Related to this:
// https://stackoverflow.com/questions/11376288/fast-computing-of-log2-for-64-bit-integers

#ifdef _MSC_VER

#include <intrin.h>
int __builtin_clz(unsigned int value)
{
    unsigned long r;
    _BitScanReverse(&r, value);
    return (31 - r);
}

#endif

int ff_flif16_ilog2(uint32_t l)
{
    if (l == 0)
        return 0;
    return sizeof(unsigned int) * 8 - 1 - __builtin_clz(l);
}

static uint32_t log4kf(int x, uint32_t base)
{
    int bits     = 8 * sizeof(int) - __builtin_clz(x);
    uint64_t y   = ((uint64_t)x) << (32 - bits);
    uint32_t res = base * (13 - bits);
    uint32_t add = base;
    while ((add > 1) && ((y & 0x7FFFFFFF) != 0)) {
        y = (((uint64_t)y) * y + 0x40000000) >> 31;
        add >>= 1;
        if ((y >> 32) != 0) {
            res -= add;
            y >>= 1;
        }
    }
    return res;
}

void ff_flif16_build_log4k_table(FLIF16RangeCoder *rc)
{
    rc->log4k->table[0] = 0;
    for (int i = 1; i <= 4096; i++)
        rc->log4k->table[i] = (log4kf(i, (65535UL << 16) / 12) + 
                               (1 << 15)) >> 16;
    rc->log4k->scale = 65535 / 12;
}

