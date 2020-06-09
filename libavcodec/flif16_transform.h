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
#define CLIP(x,l,u) ((x) > (u)) ? (u) : ((x) < (l) ? (l) : (x))

typedef int16_t FLIF16ColorVal;

// This may be useless
/*
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
*/

typedef struct transform_priv_ycocg{
    int origmax4;
    FLIF16ColorRanges* ranges;
}transform_priv_ycocg;

typedef struct transform_priv_permuteplanes{
    uint8_t subtract;
    uint8_t permutation[5];
    FLIF16ColorRanges* ranges;

    uint8_t from[4], to[4];
    FLIF16ChanceContext *ctx_a;
}transform_priv_permuteplanes;

typedef struct transform_priv_channelcompact{
    FLIF16ColorVal* CPalette[4];
    unsigned int CPalette_size[4];
    FLIF16ColorVal* CPalette_inv[4];
    unsigned int CPalette_inv_size[4];

    FLIF16ColorVal min;
    unsigned int i;                   //Iterator for nested loop.
    FLIF16ChanceContext *ctx_a;
}transform_priv_channelcompact;

typedef struct transform_priv_bounds{
    FLIF16ColorVal *bounds[2];
    FLIF16ColorVal min;
    FLIF16ChanceContext *ctx_a;
}transform_priv_bounds;

typedef struct ranges_priv_channelcompact{
    int nb_colors[4];
}ranges_priv_channelcompact;

typedef struct ranges_priv_ycocg{
    int origmax4;
    FLIF16ColorRanges* ranges;
}ranges_priv_ycocg;

typedef struct ranges_priv_permuteplanes{
    uint8_t permutation[5];
    FLIF16ColorRanges* ranges;
}ranges_priv_permuteplanes;

typedef struct ranges_priv_bounds{
    FLIF16ColorVal* bounds[2];
    FLIF16ColorRanges* ranges;
}ranges_priv_bounds;

FLIF16ColorRanges* ff_get_ranges( FLIF16InterimPixelData *pixelData,
                                  FLIF16ColorRanges *ranges);

static inline FLIF16ColorVal ff_get_max_y(int origmax4)
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

static inline void ff_default_minmax(FLIF16ColorRanges *ranges ,const int p,
                                  FLIF16ColorVal* prevPlanes,
                                  FLIF16ColorVal* minv, FLIF16ColorVal* maxv)
{
    *minv = ranges->min(ranges, p);
    *maxv = ranges->max(ranges, p);
}

static inline void ff_default_snap(FLIF16ColorRanges *ranges ,const int p,
                                FLIF16ColorVal* prevPlanes,
                                FLIF16ColorVal* minv, FLIF16ColorVal* maxv, 
                                FLIF16ColorVal* v)
{
    ff_default_minmax(ranges, p, prevPlanes, minv, maxv);
    *v = CLIP(*v, *minv, *maxv);
}

static inline FLIF16ColorVal ff_channelcompact_min(FLIF16ColorRanges* ranges, 
                                                   int p){
    return 0;
}

static inline FLIF16ColorVal ff_channelcompact_max(FLIF16ColorRanges* ranges,
                                                   int p){
    ranges_priv_channelcompact* data = ranges->priv_data;
    return data->nb_colors[p];
}

static inline void ff_channelcompact_minmax(FLIF16ColorRanges *ranges, int p,
                                         FLIF16ColorVal* prevPlanes,
                                         FLIF16ColorVal* minv,
                                         FLIF16ColorVal* maxv)
{
    ranges_priv_channelcompact* data = ranges->priv_data;
    *minv = 0;
    *maxv = data->nb_colors[p];
}

static inline FLIF16ColorVal ff_ycocg_min(FLIF16ColorRanges* ranges, int p)
{   
    ranges_priv_ycocg* data = ranges->priv_data;
    switch(p) {
        case 0:
            return 0;
        case 1:
            return -4 * data->origmax4 + 1;
        case 2:
            return -4 * data->origmax4 + 1;
        default:
            ranges->min(ranges, p);
    }
}

static inline FLIF16ColorVal ff_ycocg_max(FLIF16ColorRanges* ranges, int p)
{
    ranges_priv_ycocg* data = ranges->priv_data;
    switch(p) {
        case 0:
            return 4 * data->origmax4 - 1;
        case 1:
            return 4 * data->origmax4 - 1;
        case 2:
            return 4 * data->origmax4 - 1;
        default:
            ranges->max(ranges, p);
    }
}

static inline void ff_ycocg_minmax(FLIF16ColorRanges *ranges ,const int p,
                                FLIF16ColorVal* prevPlanes,
                                FLIF16ColorVal* minv,
                                FLIF16ColorVal* maxv)
{
    ranges_priv_ycocg* data = ranges->priv_data;
    switch(p){
        case 0:
            *minv = 0;
            *maxv = ff_get_max_y(data->origmax4);
            break;
        case 1:
            *minv = ff_get_min_co(data->origmax4, prevPlanes[0]);
            *maxv = ff_get_max_co(data->origmax4, prevPlanes[0]);
            break;    
        case 2:
            *minv = ff_get_min_cg( data->origmax4, prevPlanes[0], prevPlanes[1]);
            *maxv = ff_get_max_cg( data->origmax4, prevPlanes[0], prevPlanes[1]);
            break;
        default:
            data->ranges->minmax(data->ranges, p, prevPlanes, minv, maxv);
    }
}

static inline FLIF16ColorVal ff_permuteplanessubtract_min(
                                            FLIF16ColorRanges* ranges,
                                            int p)
{
    transform_priv_permuteplanes* data = ranges->priv_data;
    if(p==0 || p>2)
        return data->ranges->min(data->ranges, data->permutation[p]);
    return data->ranges->min(data->ranges, data->permutation[p]) - 
           data->ranges->max(data->ranges, data->permutation[0]);
}

static inline FLIF16ColorVal ff_permuteplanessubtract_max(
                                            FLIF16ColorRanges* ranges,
                                            int p)
{
    transform_priv_permuteplanes* data = ranges->priv_data;
    if(p==0 || p>2)
        return data->ranges->max(data->ranges, data->permutation[p]);
    return data->ranges->max(data->ranges, data->permutation[p]) - 
           data->ranges->min(data->ranges, data->permutation[0]);
}

static inline FLIF16ColorVal ff_permuteplanes_min(
                                            FLIF16ColorRanges* ranges,
                                            int p)
{
    transform_priv_permuteplanes* data = ranges->priv_data;
    return ranges->min(ranges, data->permutation[p]);
}

static inline FLIF16ColorVal ff_permuteplanes_max(
                                            FLIF16ColorRanges* ranges,
                                            int p)
{
    transform_priv_permuteplanes* data = ranges->priv_data;
    return ranges->max(ranges, data->permutation[p]);
}

static inline void ff_permuteplanessubtract_minmax(
                                    FLIF16ColorRanges* ranges, int p,
                                    FLIF16ColorVal* prevPlanes, 
                                    FLIF16ColorVal* minv, FLIF16ColorVal* maxv)
{
    ranges_priv_permuteplanes* data = ranges->priv_data;
    if(p==0 || p>2){
        *minv = ranges->min(ranges, p);
        *maxv = ranges->max(ranges, p);
    }
    else{
        *minv = ranges->min(ranges, data->permutation[p]) - prevPlanes[0];
        *maxv = ranges->max(ranges, data->permutation[p]) - prevPlanes[0];
    }
}

/*
static inline FLIF16ColorVal ff_static_min(FLIF16ColorRanges* ranges, int p)
{
    if(p >= ranges->num_planes)
        return 0;
    assert(p < ranges->num_planes);
    return 
}
*/


uint8_t ff_flif16_transform_ycocg_init(FLIF16TransformContext*,
                                       FLIF16ColorRanges*);
uint8_t ff_flif16_transform_ycocg_forward(FLIF16TransformContext*,
                                          FLIF16InterimPixelData*);
uint8_t ff_flif16_transform_ycocg_reverse(FLIF16TransformContext*,
                                          FLIF16InterimPixelData*,
                                          uint32_t, uint32_t);

uint8_t ff_flif16_transform_permuteplanes_read(FLIF16TransformContext*,
                                               FLIF16DecoderContext*,
                                               FLIF16ColorRanges*);
uint8_t ff_flif16_transform_permuteplanes_init(FLIF16TransformContext*,
                                               FLIF16ColorRanges*);
uint8_t ff_flif16_transform_permuteplanes_forward(
                                            FLIF16TransformContext*,
                                            FLIF16InterimPixelData*);
uint8_t ff_flif16_transform_permuteplanes_reverse(
                                        FLIF16TransformContext*,
                                        FLIF16InterimPixelData*,
                                        uint32_t, uint32_t);

uint8_t ff_flif16_transform_channelcompact_read(FLIF16TransformContext*,
                                               FLIF16DecoderContext*,
                                               FLIF16ColorRanges*);
uint8_t ff_flif16_transform_channelcompact_init(FLIF16TransformContext*,
                                                FLIF16ColorRanges*);
//uint8_t ff_flif16_transform_channelcompact_forward(
//                                            FLIF16TransformContext *ctx,
//                                            FLIF16DecoderContext *dec_ctx,
//                                            FLIF16InterimPixelData * pixelData);
uint8_t ff_flif16_transform_channelcompact_reverse(
                                        FLIF16TransformContext*,
                                        FLIF16InterimPixelData*,
                                        uint32_t, uint32_t);

uint8_t ff_flif16_transform_bounds_read(FLIF16TransformContext*,
                                        FLIF16DecoderContext*,
                                        FLIF16ColorRanges*);
uint8_t ff_flif16_transform_bounds_init(FLIF16TransformContext*,
                                        FLIF16ColorRanges*);

uint8_t ff_flif16_transform_read(FLIF16TransformContext*, 
                                FLIF16DecoderContext*,
                                FLIF16ColorRanges*);
FLIF16TransformContext *ff_flif16_transform_init(int, 
                                                 FLIF16DecoderContext*);

#endif /* FLIF16_TRANSFORM_H */
