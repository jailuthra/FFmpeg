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

#include "avcodec.h"
#include "flif16_rangecoder.h"

// Remove this
#define __PLN__ printf("At: [%s] %s, %d\n", __func__, __FILE__, __LINE__);

#define FF_FLIF16_VARINT_APPEND(a,x) a = (a << 7) | (uint64_t) (x & 127)

#define CHANCETABLE_DEFAULT_ALPHA (0xFFFFFFFF / 19)
#define CHANCETABLE_DEFAULT_CUT 2

#define RANGE_MIN(ranges, channels, p) (((p) > (channels)) ? 0 : (ranges)[p][0])
#define RANGE_MAX(ranges, channels, p) (((p) > (channels)) ? 0 : (ranges)[p][1])
#define RANGE_SET(range, l, h) (range[0] = l, range[1] = h)

#define MAX_PLANES 5

static const uint8_t flif16_header[4] = "FLIF";

struct FLIF16DecoderContext;
typedef struct FLIF16DecoderContext FLIF16DecoderContext;

typedef struct FLIF16DecoderContext {
    GetByteContext gb;
    FLIF16MANIACContext maniac_ctx;
    FLIF16RangeCoder *rc;
    AVFrame *out_frames;
    
    uint8_t buf[FLIF16_RAC_MAX_RANGE_BYTES]; ///< Storage for initial RAC buffer
    uint8_t buf_count;    ///< Count for initial RAC buffer
    int state;            ///< The section of the file the parser is in currently.
    unsigned int segment; ///< The "segment" the code is supposed to jump to
    int i;                ///< A generic iterator used to save states between
                          ///  for loops.
    // Primary Header     
    uint8_t ia;           ///< Is image interlaced or/and animated or not
    uint32_t bpc;         ///< Bytes per channel
    uint8_t channels;     ///< Number of channels
    uint8_t varint;       ///< Number of varints to process in sequence
                          
    // Secondary Header
    
    // Flags. TODO Merge all these flags
    uint8_t alphazero;    ///< Alphazero
    uint8_t custombc;     ///< Custom Bitchance

    uint8_t cutoff; 
    uint8_t alphadiv;

    uint8_t loops;        ///< Number of times animation loops
    uint16_t *framedelay; ///< Frame delay for each frame
    int32_t (*ranges)[2]; ///< The minimum and maximum values a
                          ///  channel's pixels can take. Changes
                          ///  depending on transformations applied
    int32_t (*prop_ranges)[2];
    uint32_t prop_ranges_size;

    // Transforms
    // Size dynamically maybe
    // FLIF16TransformContext *transforms[13];
    uint8_t transform_top;
    //FLIF16ColorRanges ranges;
    //FLIF16ColorRanges ranges_prev;
    
    // Dimensions and other things.
    uint32_t width;
    uint32_t height;
    uint32_t frames;
    uint32_t meta;      ///< Size of a meta chunk
} FLIF16DecoderContext;

void ff_flif16_maniac_ni_prop_ranges_init(int32_t (*prop_ranges)[2],
                                          unsigned int *prop_ranges_size,
                                          int32_t (*ranges)[2],
                                          uint8_t property,
                                          uint8_t channels);

// Must be included here to resolve circular include
#include "flif16_transform.h"

#endif /* AVCODEC_FLIF16_H */
