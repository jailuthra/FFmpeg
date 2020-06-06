/*
 * Transforms for FLIF16.
 * Copyright (c) 2020 Kartik K. Khullar <kartikkhullar840@gmail.com>
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
 * Transforms for FLIF16.
 */
 
#ifndef FLIF16_TRANSFORM_H
#define FLIF16_TRANSFORM_H

#include <stdint.h>

#include "avcodec.h"
#include "flif16.h"
#include "libavutil/common.h"

#define MAX_PLANES 5

// Replace by av_clip functions
#define CLIP(x,l,u) (x) > (u) ? (u) : ((x) < (l) ? (l) : (x))

typedef int16_t FLIF16ColorVal;

// This may be useless
typedef enum FLIF16TransformTypes {
    FLIF16_TRANSFORM_CHANNELCOMPACT = 0,
    FLIF16_TRANSFORM_YCOCG,
    FLIF16_TRANSFORM_RESERVED1,
    FLIF16_TRANSFORM_PERMUTEPLANES,
    FLIF16_TRANSFORM_BOUNDS,
    FLIF16_TRANSFORM_PALETTEALPHA,
    FLIF16_TRANSFORM_PALETTE,
    FLIF16_TRANSFORM_COLORBUCKETS,
    FLIF16_TRANSFORM_RESERVED2,
    FLIF16_TRANSFORM_RESERVED3,
    FLIF16_TRANSFORM_DUPLICATEFRAME,
    FLIF16_TRANSFORM_FRAMESHAPE,
    FLIF16_TRANSFORM_FRAMELOOKBACK
};

typedef struct FLIF16TransformContext{
    uint8_t t_no;
    unsigned int segment;
    int i;
    size_t priv_data_size;
    uint8_t done;
    void *priv_data;
}FLIF16TransformContext;

typedef struct FLIF16Transform {
    uint8_t priv_data_size;
    //Functions
    uint8_t (*init) (FLIF16TransformContext*, FLIF16DecoderContext*);
    uint8_t (*read) (FLIF16TransformContext*, FLIF16DecoderContext*);
    uint8_t (*forward) (FLIF16TransformContext*, FLIF16DecoderContext*, 
                        FLIF16InterimPixelData*);
    uint8_t (*reverse) (FLIF16TransformContext*, FLIF16DecoderContext*, 
                        FLIF16InterimPixelData*, 
                        uint32_t, uint32_t);
} FLIF16Transform;

typedef struct transform_priv_ycocg{
    int origmax4;
    FLIF16ColorRanges ranges;
}transform_priv_ycocg;

typedef struct transform_priv_permuteplanes{
    uint8_t subtract;
    uint8_t permutation[5];
    FLIF16ColorRanges ranges;
    uint8_t from[4], to[4];
    FLIF16ChanceContext ctx_a;
}transform_priv_permuteplanes;

typedef struct transform_priv_channelcompact{
    FLIF16ColorVal* CPalette[4];
    unsigned int CPalette_size[4];
    FLIF16ColorVal* CPalette_inv[4];
    unsigned int CPalette_inv_size[4];
    FLIF16ColorVal min;
    unsigned int i;                   //Iterator for nested loop.
    FLIF16ChanceContext ctx_a;
}transform_priv_channelcompact;

FLIF16ColorRanges* ff_get_ranges( FLIF16InterimPixelData *pixelData,
                                  FLIF16ColorRanges *ranges);

/*
FLIF16ColorRanges ff_get_crange_ycocg(  int p,
                                FLIF16ColorVal* prevPlanes,
                                FLIF16Transform transform ){
    FLIF16ColorRanges crange;
    switch(p){
        case 0:
            crange.min[0] = get_min_y(transform.origmax4);
            crange.max[0] = get_max_y(transform.origmax4);
            break;
        case 1:
            crange.min[1] = get_min_co(transform.origmax4, prevPlanes[0]);
            crange.max[1] = get_max_co(transform.origmax4, prevPlanes[0]);
            break;    
        case 2:
            crange.min[2] = get_min_cg(  transform.origmax4,
                                         prevPlanes[0],
                                         prevPlanes[1]);

            crange.max[2] = get_max_cg(  transform.origmax4,
                                         prevPlanes[0],
                                         prevPlanes[1]);
            break;
        default:
            break; 
    }
    return crange;
}
*/

// Some internal functions for YCoCg Transform.
static inline int ff_get_min_y(int origmax4)
{
    return 0;
}

static inline int ff_get_max_y(int origmax4)
{
    return 4 * origmax4-1;
}

static inline int ff_get_min_co(int origmax4, int yval)
{
    int newmax = 4 * origmax4 - 1;
    if (yval < origmax4 - 1)
        return -3 - 4 * yval; 
    else if (yval >= 3 * origmax4)
        return 4 * (yval - newmax);
    else
        return -newmax;
}

static inline int ff_get_max_co(int origmax4, int yval)
{
    int newmax = 4 * origmax4 - 1;
    if (yval < origmax4-1)
        return 3 + 4 * yval; 
    else if (yval >= 3 * origmax4)
        return 4 * (newmax - yval);
    else
        return newmax;
}

static inline int ff_get_min_cg(int origmax4, int yval, int coval)
{
    int newmax = 4 * origmax4 - 1;
    if (yval < origmax4 - 1)
        return -2 - 2 * yval; 
    else if (yval >= 3 * origmax4)
        return -2 * (newmax - yval) + 2 * ((abs(coval) + 1) / 2);
    else{
        return FFMIN(2 * yval + 1, 2 * newmax - 2 * yval - 2 * abs(coval) + 1) \
               / 2;
    }
}

static inline int ff_get_max_cg(int origmax4, int yval, int coval){
    int newmax = 4 * origmax4 - 1;
    if (yval < origmax4 - 1)
        return 1 + 2 * yval - 2 * (abs(coval) / 2); 
    else if (yval >= 3 * origmax4)
        return 2 * (newmax - yval);
    else
        return FFMIN(2 * (yval- newmax), -2 * yval - 1 + 2 * (abs(coval) / 2));
}

static inline int ff_min_range_ycocg(int p, int origmax4)
{
    switch(p) {
        case 0:
            return 0;
        case 1:
            return -4 * origmax4 + 1;
        case 2:
            return -4 * origmax4 + 1;
        default:
            return 0;
    }
}

static inline int ff_max_range_ycocg(int p, int origmax4)
{
    switch(p) {
        case 0:
            return 4 * origmax4 - 1;
        case 1:
            return 4 * origmax4 - 1;
        case 2:
            return 4 * origmax4 - 1;
        default:
            return 0;
    }
}

uint8_t ff_flif16_transform_ycocg_init(FLIF16TransformContext *ctx,
                                       FLIF16DecoderContext *dec_ctx);
uint8_t ff_flif16_transform_ycocg_forward(FLIF16TransformContext *ctx,
                                          FLIF16DecoderContext *dec_ctx,
                                          FLIF16InterimPixelData * pixelData);
uint8_t ff_flif16_transform_ycocg_reverse(FLIF16TransformContext *ctx,
                                          FLIF16DecoderContext *dec_ctx,
                                          FLIF16InterimPixelData * pixelData,
                                          uint32_t strideRow,
                                          uint32_t strideCol);

uint8_t ff_flif16_transform_permuteplanes_read(FLIF16TransformContext * ctx,
                                               FLIF16DecoderContext *dec_ctx);
uint8_t ff_flif16_transform_permuteplanes_init(FLIF16TransformContext *ctx,
                                               FLIF16DecoderContext *dec_ctx);
uint8_t ff_flif16_transform_permuteplanes_forward(
                                            FLIF16TransformContext *ctx,
                                            FLIF16DecoderContext *dec_ctx,
                                            FLIF16InterimPixelData * pixelData);
uint8_t ff_flif16_transform_permuteplanes_reverse(
                                        FLIF16TransformContext *ctx,
                                        FLIF16DecoderContext *dec_ctx,
                                        FLIF16InterimPixelData * pixelData,
                                        uint32_t strideRow,
                                        uint32_t strideCol);

uint8_t ff_flif16_transform_channelcompact_read(FLIF16TransformContext * ctx,
                                               FLIF16DecoderContext *dec_ctx);
uint8_t ff_flif16_transform_channelcompact_init(FLIF16TransformContext *ctx,
                                               FLIF16DecoderContext *dec_ctx);
//uint8_t ff_flif16_transform_channelcompact_forward(
//                                            FLIF16TransformContext *ctx,
//                                            FLIF16DecoderContext *dec_ctx,
//                                            FLIF16InterimPixelData * pixelData);
uint8_t ff_flif16_transform_channelcompact_reverse(
                                        FLIF16TransformContext *ctx,
                                        FLIF16DecoderContext *dec_ctx,
                                        FLIF16InterimPixelData * pixelData,
                                        uint32_t strideRow,
                                        uint32_t strideCol);

int ff_flif16_transform_read(FLIF16TransformContext *c, 
                             FLIF16DecoderContext *s);
FLIF16TransformContext *ff_flif16_transform_init(int t_no, 
                                                 FLIF16DecoderContext *s);

#endif
