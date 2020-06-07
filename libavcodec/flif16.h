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

#define MANIAC_TREE_BASE_SIZE 16
#define MANIAC_TREE_MIN_COUNT 1
#define MANIAC_TREE_MAX_COUNT 512
#define FF_FLIF16_VARINT_APPEND(a,x) a = (a << 7) | (uint64_t) (x & 127)
#define RANGE_MIN(ranges, channels, p) (((p) > (channels)) ? 0 : (ranges)[p][0])
#define RANGE_MAX(ranges, channels, p) (((p) > (channels)) ? 0 : (ranges)[p][1])
#define RANGE_SET(range, l, h) (range[0] = l, range[1] = h)

#define MAX_PLANES 5

static const uint8_t flif16_header[4] = "FLIF";

typedef int16_t FLIF16ColorVal;

typedef struct FLIF16ColorRanges {
    FLIF16ColorVal min[MAX_PLANES], max[MAX_PLANES];
    int num_planes;
} FLIF16ColorRanges;

typedef struct FLIF16InterimPixelData {
    uint8_t initialized;            //FLAG : initialized or not.
    int height, width;
    FLIF16ColorVal *data[MAX_PLANES];
    FLIF16ColorRanges ranges;
} FLIF16InterimPixelData;

typedef struct FLIF16MANIACStack {
    unsigned int id;
    int p;
    int min;
    int max;
    int max2;
    uint8_t mode;
    uint8_t visited;
} FLIF16MANIACStack;

typedef struct FLIF16MANIACNode {
    int8_t property;         
    int16_t count;
    // typedef int32_t ColorVal; 
    int32_t split_val;
    uint32_t child_id;
    uint32_t leaf_id;
    // probably safe to use only uint16
    //uint16_t childID;
    //uint16_t leafID;
    // PropertyDecisionNode(int p=-1, int s=0, int c=0) : property(p), count(0), splitval(s), childID(c), leafID(0) {}
} FLIF16MANIACNode;

typedef struct FLIF16MANIACContext {
    FLIF16MANIACNode *tree;
    FLIF16MANIACStack *stack;
    FLIF16ChanceContext *ctx[3];
    unsigned int tree_top;
    unsigned int tree_size;
    unsigned int stack_top;
    unsigned int stack_size;
} FLIF16MANIACContext;

typedef struct FLIF16DecoderContext {
    GetByteContext gb;
    FLIF16MANIACContext maniac_ctx;
    FLIF16RangeCoder *rc;
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
    
    // Dimensions and other things.
    uint32_t width;
    uint32_t height;
    uint32_t frames;
    uint32_t meta;      ///< Size of a meta chunk
    FLIF16ColorRanges src_ranges;
} FLIF16DecoderContext;

#endif /* AVCODEC_FLIF16_H */
