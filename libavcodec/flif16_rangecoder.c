/*
 * Range coder for FLIF16
 * Copyright (c) 2004, 2020 Jon Sneyers, Michael Niedermayer, Anamitra Ghorui
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
  * Range coder for FLIF16
  */

#include "flif16_rangecoder.h"

// TODO write separate function for RAC decoder

// The coder requires a certain number of bytes for initiialization. buf
// provides it. gb is used by the coder functions for actual coding.

FLIF16RangeCoder *ff_flif16_rac_init(GetByteContext *gb, 
                                     uint8_t *buf,
                                     uint8_t buf_size)
{
    FLIF16RangeCoder *rc = av_mallocz(sizeof(*rc));
    GetByteContext gbi;

    if (!rc)
        return NULL;
    
    if(buf_size < FLIF16_RAC_MAX_RANGE_BYTES)
        return NULL;
    
    bytestream2_init(&gbi, buf, buf_size);

    rc->range  = FLIF16_RAC_MAX_RANGE;
    rc->gb     = gb;

    for (uint32_t r = FLIF16_RAC_MAX_RANGE; r > 1; r >>= 8) {
        rc->low <<= 8;
        rc->low |= bytestream2_get_byte(&gbi);
    }
    printf("[%s] low = %d\n", __func__, rc->low);
    return rc;
}

void ff_flif16_rac_free(FLIF16RangeCoder *rc)
{
    free(rc->ct);
    free(rc->log4k);
    free(rc);
}

// TODO Maybe restructure rangecoder.c/h to fit a more generic case
static void build_table(uint16_t *zero_state, uint16_t *one_state, size_t size,
                        uint32_t factor, unsigned int max_p)
{
    const int64_t one = 1LL << 32;
    int64_t p = one / 2;
    unsigned int last_p8 = 0, p8;
    unsigned int i;

    for (i = 0; i < size / 2; i++) {
        p8 = (size * p + one / 2) >> 32;
        if (p8 <= last_p8) 
            p8 = last_p8 + 1;
        if (last_p8 && last_p8 < size && p8 <= max_p)
            one_state[last_p8] = p8;
        p += ((one - p) * factor + one / 2) >> 32;
        last_p8 = p8;
    }

    for (i = size - max_p; i <= max_p; i++) {
        if (one_state[i])
            continue;
        p = (i * one + size / 2) / size;
        p += ((one - p) * factor + one / 2) >> 32;
        p8 = (size * p + one / 2) >> 32; //FIXME try without the one
        if (p8 <= i) 
            p8 = i + 1;
        if (p8 > max_p) 
            p8 = max_p;
        one_state[i] = p8;
    }

    for (i = 1; i < size; i++)
        zero_state[i] = size - one_state[size - i];
}

static uint32_t log4kf(int x, uint32_t base)
{
    int bits     = 8 * sizeof(int) - ff_clz(x);
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
    for (int i = 1; i < 4096; i++)
        rc->log4k->table[i] = (log4kf(i, (65535UL << 16) / 12) + 
                               (1 << 15)) >> 16;
    rc->log4k->scale = 65535 / 12;
}

void ff_flif16_chancetable_init(FLIF16RangeCoder *rc, int alpha, int cut) {
    rc->chance = 0x800;
    rc->ct = av_mallocz(sizeof(*(rc->ct)));
    build_table(rc->ct->zero_state, rc->ct->one_state, 4096, alpha, 4096 - cut);
    ff_flif16_build_log4k_table(rc);
}
