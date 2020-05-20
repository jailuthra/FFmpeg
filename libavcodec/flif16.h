/*
 * FLIF16 Image Format Definitions
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
 * FLIF16 format definitions and functions.
 */

#ifndef AVCODEC_FLIF16_H
#define AVCODEC_FLIF16_H

#include <stdint.h>
#include <stdlib.h>
// Remove this
#define __PLN__ printf("At: [%s] %s, %d\n", __func__, __FILE__, __LINE__);
static const uint8_t flif16_header[4] = "FLIF";
#define FF_FLIF16_VARINT_APPEND(a,x) a = (a << 7) | (uint64_t) (x & 127)

void ff_flif16_build_log4k_table(FLIF16RangeCoder *ct);
int ff_flif16_ilog2(uint32_t l);

#endif /* AVCODEC_FLIF16_H */
