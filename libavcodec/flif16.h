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
#include "flif16_rangecoder.h"
// Remove this
#define __PLN__ printf("At: [%s] %s, %d\n", __func__, __FILE__, __LINE__);
#define FF_FLIF16_VARINT_APPEND(a,x) a = (a << 7) | (uint64_t) (x & 127)

static const uint8_t flif16_header[4] = "FLIF";

typedef struct FLIF16DecoderContext {
    GetByteContext gb;
    FLIF16RangeCoder *rc;
    uint8_t buf[FLIF16_RAC_MAX_RANGE_BYTES]; ///< Storage for initial RAC buffer
    uint8_t buf_count;   ///< Count for initial RAC buffer
    int state;           ///< The section of the file the parser is in currently.
    unsigned int segment;///< The "segment" the code is supposed to jump to
    int i;               ///< A generic iterator used to save states between
                         ///  for loops.
    // Primary Header
    uint8_t ia;          ///< Is image interlaced or/and animated or not
    uint8_t bpc;         ///< Bytes per channel
    uint8_t channels;    ///< Number of channels
    uint8_t varint;      ///< Number of varints to process in sequence
    
    // Secondary Header
    uint8_t  channelbpc; ///< bpc per channel. Size == 1 if bpc == '0' 
                         ///  else equal to number of frames
    
    // Flags. TODO Merge all these flags
    uint8_t alphazero;   ///< Alphazero
    uint8_t custombc;    ///< Custom Bitchance

    uint8_t cutoff; 
    uint8_t alphadiv;

    uint8_t loops;       ///< Number of times animation loops
    uint16_t *framedelay;///< Frame delay for each frame
    
    // Dimensions and other things.
    uint32_t width;
    uint32_t height;
    uint32_t frames;
    uint32_t meta;      ///< Size of a meta chunk
} FLIF16DecoderContext;

#endif /* AVCODEC_FLIF16_H */
