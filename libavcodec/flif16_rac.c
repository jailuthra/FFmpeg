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
  * Range coder for FLIF16
  */

#include "flif16_rac.h"

FLIF16RangeCoder *ff_flif16_rac_init(GetByteContext *gb)
{
    FLIF16RangeCoder *rc = av_mallocz(sizeof(*k));
    if (!rc)
        return NULL;

    rc->range  = FLIF16_RAC_MAX_RANGE;
    rc->gb     = gb;
    uint32_t r = FLIF16_RAC_MIN_RANGE;
    while (r > 1) {
        rc->low <<= 8;
        rc->low |= bytestream2_get_byte(k->gb);
        r >>= 8;
    }
    return k;
}


