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

typedef enum FLIF16ColorRangesTypes{
    FLIF16_COLORRANGES_DEFAULT,
    FLIF16_COLORRANGES_CHANNELCOMPACT = 0,
    FLIF16_COLORRANGES_YCOCG,
    FLIF16_COLORRANGES_PERMUTEPLANES,
    FLIF16_COLORRANGES_PERMUTEPLANESSUBTRACT,
    FLIF16_COLORRANGES_BOUNDS,
    FLIF16_COLORRANGES_STATIC,
    FLIF16_COLORRANGES_PALETTEALPHA,
    FLIF16_COLORRANGES_PALETTE,
    FLIF16_COLORRANGES_COLORBUCKETS,
    FLIF16_COLORRANGES_DUPLICATEFRAME,
    FLIF16_COLORRANGES_FRAMESHAPE,
    FLIF16_COLORRANGES_FRAMELOOKBACK,
    FLIF16_COLORRANGES_DUP
}FLIF16ColorRangesTypes;

static uint8_t ff_flif16_transform_ycocg_init(FLIF16TransformContext*,
                                              FLIF16ColorRangesContext*);
FLIF16ColorRangesContext* ff_flif16_transform_ycocg_meta(
                                                         FLIF16TransformContext*,
                                                         FLIF16ColorRangesContext*);                                       
static uint8_t ff_flif16_transform_ycocg_forward(FLIF16TransformContext*,
                                                 FLIF16InterimPixelData*);
static uint8_t ff_flif16_transform_ycocg_reverse(FLIF16TransformContext*,
                                                 FLIF16InterimPixelData*,
                                                 uint32_t, uint32_t);

static uint8_t ff_flif16_transform_permuteplanes_read(FLIF16TransformContext*,
                                                      FLIF16DecoderContext*,
                                                      FLIF16ColorRangesContext*);
static uint8_t ff_flif16_transform_permuteplanes_init(FLIF16TransformContext*,
                                                      FLIF16ColorRangesContext*);
static FLIF16ColorRangesContext* ff_flif16_transform_permuteplanes_meta(
                                                     FLIF16TransformContext*,
                                                     FLIF16ColorRangesContext*);
static uint8_t ff_flif16_transform_permuteplanes_forward(
                                                      FLIF16TransformContext*,
                                                       FLIF16InterimPixelData*);
static uint8_t ff_flif16_transform_permuteplanes_reverse(
                                                        FLIF16TransformContext*,
                                                        FLIF16InterimPixelData*,
                                                        uint32_t, uint32_t);

static uint8_t ff_flif16_transform_channelcompact_read(FLIF16TransformContext*,
                                                       FLIF16DecoderContext*,
                                                       FLIF16ColorRangesContext*);
static uint8_t ff_flif16_transform_channelcompact_init(FLIF16TransformContext*,
                                                       FLIF16ColorRangesContext*);
FLIF16ColorRangesContext* ff_flif16_transform_channelcompact_meta(
                                                     FLIF16TransformContext*,
                                                     FLIF16ColorRangesContext*);                                                
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
                                        FLIF16ColorRangesContext*);
uint8_t ff_flif16_transform_bounds_init(FLIF16TransformContext*,
                                        FLIF16ColorRangesContext*);
FLIF16ColorRangesContext* ff_flif16_transform_bounds_meta(
                                    FLIF16TransformContext*,
                                    FLIF16ColorRangesContext*);

typedef struct transform_priv_ycocg{
    int origmax4;
    FLIF16ColorRangesContext* r_ctx;
}transform_priv_ycocg;

typedef struct transform_priv_permuteplanes{
    uint8_t subtract;
    uint8_t permutation[5];
    FLIF16ColorRangesContext* r_ctx;

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
    FLIF16ColorRangesContext* r_ctx;
}ranges_priv_ycocg;

typedef struct ranges_priv_permuteplanes{
    uint8_t permutation[5];
    FLIF16ColorRangesContext* r_ctx;
}ranges_priv_permuteplanes;

typedef struct ranges_priv_bounds{
    FLIF16ColorVal* bounds[2];
    FLIF16ColorRangesContext* r_ctx;
}ranges_priv_bounds;

typedef struct ranges_priv_static{
    FLIF16ColorVal* bounds[2];
}ranges_priv_static;

FLIF16Transform flif16_transform_channelcompact = {
    .priv_data_size = sizeof(transform_priv_channelcompact),
    .init           = &ff_flif16_transform_channelcompact_init,
    .read           = &ff_flif16_transform_channelcompact_read,
    .meta           = &ff_flif16_transform_channelcompact_meta,
    //.forward        = &ff_flif16_transform_channelcompact_forward,
    .reverse        = &ff_flif16_transform_channelcompact_reverse 
};

FLIF16Transform flif16_transform_ycocg = {
    .priv_data_size = sizeof(transform_priv_ycocg),
    .init           = &ff_flif16_transform_ycocg_init,
    .read           = NULL,
    .meta           = &ff_flif16_transform_ycocg_meta,
    .forward        = &ff_flif16_transform_ycocg_forward,
    .reverse        = &ff_flif16_transform_ycocg_reverse 
};

FLIF16Transform flif16_transform_permuteplanes = {
    .priv_data_size = sizeof(transform_priv_permuteplanes),
    .init           = &ff_flif16_transform_permuteplanes_init,
    .read           = &ff_flif16_transform_permuteplanes_read,
    .meta           = &ff_flif16_transform_permuteplanes_meta,
    .forward        = &ff_flif16_transform_permuteplanes_forward,
    .reverse        = &ff_flif16_transform_permuteplanes_reverse 
};

FLIF16Transform flif16_transform_bounds = {
    .priv_data_size = sizeof(transform_priv_bounds),
    .init           = &ff_flif16_transform_bounds_init,
    .read           = &ff_flif16_transform_bounds_read,
    .meta           = &ff_flif16_transform_bounds_meta
};

FLIF16Transform *flif16_transforms[13] = {
    &flif16_transform_channelcompact,
    &flif16_transform_ycocg,
    NULL, // FLIF16_TRANSFORM_RESERVED1,
    &flif16_transform_permuteplanes,
    &flif16_transform_bounds,
    NULL, // FLIF16_TRANSFORM_PALETTEALPHA,
    NULL, // FLIF16_TRANSFORM_PALETTE,
    NULL, // FLIF16_TRANSFORM_COLORBUCKETS,
    NULL, // FLIF16_TRANSFORM_RESERVED2,
    NULL, // FLIF16_TRANSFORM_RESERVED3,
    NULL, // FLIF16_TRANSFORM_DUPLICATEFRAME,
    NULL, // FLIF16_TRANSFORM_FRAMESHAPE,
    NULL  // FLIF16_TRANSFORM_FRAMELOOKBACK
};

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

static inline void ff_default_snap(FLIF16ColorRangesContext*, int,
                                   FLIF16ColorVal*, FLIF16ColorVal*,
                                   FLIF16ColorVal*, FLIF16ColorVal*);
static inline void ff_bounds_snap(FLIF16ColorRangesContext*, int,
                                   FLIF16ColorVal*, FLIF16ColorVal*,
                                   FLIF16ColorVal*, FLIF16ColorVal*);
static inline void ff_default_minmax(FLIF16ColorRangesContext*, int,
                                     FLIF16ColorVal*, FLIF16ColorVal*,
                                     FLIF16ColorVal*);
static inline void ff_channelcompact_minmax(FLIF16ColorRangesContext*, int,
                                            FLIF16ColorVal*, FLIF16ColorVal*,
                                            FLIF16ColorVal*);
static inline void ff_ycocg_minmax(FLIF16ColorRangesContext*, int,
                                   FLIF16ColorVal*, FLIF16ColorVal*,
                                   FLIF16ColorVal*);
static inline void ff_permuteplanessubtract_minmax(
                                            FLIF16ColorRangesContext*, int,
                                            FLIF16ColorVal*, FLIF16ColorVal*,
                                            FLIF16ColorVal*);
static inline void ff_bounds_minmax(FLIF16ColorRangesContext*, int,
                                    FLIF16ColorVal*, FLIF16ColorVal*,
                                    FLIF16ColorVal*);
static inline FLIF16ColorVal ff_static_min(FLIF16ColorRangesContext*, int);
static inline FLIF16ColorVal ff_channelcompact_min(FLIF16ColorRangesContext*, int);
static inline FLIF16ColorVal ff_ycocg_min(FLIF16ColorRangesContext*, int);
static inline FLIF16ColorVal ff_permuteplanes_min(FLIF16ColorRangesContext*, int);
static inline FLIF16ColorVal ff_permuteplanessubtract_min(FLIF16ColorRangesContext*, int);
static inline FLIF16ColorVal ff_bounds_min(FLIF16ColorRangesContext*, int);

static inline FLIF16ColorVal ff_static_max(FLIF16ColorRangesContext*, int);
static inline FLIF16ColorVal ff_channelcompact_max(FLIF16ColorRangesContext*, int);
static inline FLIF16ColorVal ff_ycocg_max(FLIF16ColorRangesContext*, int);
static inline FLIF16ColorVal ff_permuteplanes_max(FLIF16ColorRangesContext*, int);
static inline FLIF16ColorVal ff_permuteplanessubtract_max(FLIF16ColorRangesContext*, int);
static inline FLIF16ColorVal ff_bounds_max(FLIF16ColorRangesContext*, int);

FLIF16ColorRanges flif16_colorranges_default = {
    .snap   = &ff_default_snap,
    .minmax = &ff_default_minmax,
    .is_static = 1,
    .min = NULL,
    .max = NULL,
    .priv_data_size = 0,
};

FLIF16ColorRanges flif16_colorranges_static = {
    .snap   = &ff_default_snap,
    .minmax = &ff_default_minmax,
    .is_static = 1,
    .min = &ff_static_min,
    .max = &ff_static_max,
    .priv_data_size = sizeof(ranges_priv_static)
};

FLIF16ColorRanges flif16_colorranges_channelcompact = {
    .snap   = &ff_default_snap,
    .minmax = &ff_channelcompact_minmax,
    .is_static = 1,
    .min = &ff_channelcompact_min,
    .max = &ff_channelcompact_max,
    .priv_data_size = sizeof(ranges_priv_channelcompact)
};

FLIF16ColorRanges flif16_colorranges_ycocg = {
    .snap   = &ff_default_snap,
    .minmax = &ff_ycocg_minmax,
    .min    = &ff_ycocg_min,
    .max    = &ff_ycocg_max,
    .is_static = 0,
    .priv_data_size = sizeof(ranges_priv_ycocg)
};

FLIF16ColorRanges flif16_colorranges_permuteplanessubtract = {
    .snap   = &ff_default_snap,
    .minmax = &ff_permuteplanessubtract_minmax,
    .min    = &ff_permuteplanessubtract_min,
    .max    = &ff_permuteplanessubtract_max,
    .is_static = 0,
    .priv_data_size = sizeof(ranges_priv_permuteplanes)
};

FLIF16ColorRanges flif16_colorranges_permuteplanes = {
    .snap   = &ff_default_snap,
    .minmax = &ff_default_minmax,
    .min    = &ff_permuteplanes_min,
    .max    = &ff_permuteplanes_max,
    .is_static = 0,
    .priv_data_size = sizeof(ranges_priv_permuteplanes)
};

FLIF16ColorRanges flif16_colorranges_bounds = {
    .snap   = &ff_bounds_snap,
    .minmax = &ff_bounds_minmax,
    .min    = &ff_bounds_min,
    .max    = &ff_bounds_max,
    .is_static = 0,
    .priv_data_size = sizeof(ranges_priv_bounds)
};

FLIF16ColorRanges* flif16_ranges[] = {
    &flif16_colorranges_default,
    &flif16_colorranges_channelcompact,
    &flif16_colorranges_ycocg,
    &flif16_colorranges_permuteplanes,
    &flif16_colorranges_permuteplanessubtract,
    &flif16_colorranges_bounds,
    &flif16_colorranges_static
};

static inline void ff_default_minmax(FLIF16ColorRangesContext *src_ctx ,const int p,
                                     FLIF16ColorVal* prevPlanes,
                                     FLIF16ColorVal* minv, FLIF16ColorVal* maxv)
{
    FLIF16ColorRanges* ranges = flif16_ranges[src_ctx->r_no];
    *minv = ranges->min(src_ctx, p);
    *maxv = ranges->max(src_ctx, p);
}

static inline void ff_default_snap(FLIF16ColorRangesContext *src_ctx ,const int p,
                                FLIF16ColorVal* prevPlanes,
                                FLIF16ColorVal* minv, FLIF16ColorVal* maxv, 
                                FLIF16ColorVal* v)
{
    ff_default_minmax(src_ctx, p, prevPlanes, minv, maxv);
    if(*minv > *maxv)
        *maxv = *minv;
    *v = av_clip(*v, *minv, *maxv);
}

static inline FLIF16ColorVal ff_static_min(FLIF16ColorRangesContext* r_ctx,
                                           int p)
{
    ranges_priv_static* data = r_ctx->priv_data;
    if(p >= r_ctx->num_planes)
        return 0;
    assert(p < r_ctx->num_planes);
    return data->bounds[0][p];
}

static inline FLIF16ColorVal ff_static_max(FLIF16ColorRangesContext* r_ctx,
                                           int p)
{
    ranges_priv_static* data = r_ctx->priv_data;
    if(p >= r_ctx->num_planes)
        return 0;
    assert(p < r_ctx->num_planes);
    return data->bounds[1][p];
}

static inline FLIF16ColorVal ff_channelcompact_min(FLIF16ColorRangesContext* ranges, 
                                                   int p){
    return 0;
}

static inline FLIF16ColorVal ff_channelcompact_max(FLIF16ColorRangesContext* src_ctx,
                                                   int p){
    ranges_priv_channelcompact* data = src_ctx->priv_data;
    return data->nb_colors[p];
}

static inline void ff_channelcompact_minmax(FLIF16ColorRangesContext *r_ctx, int p,
                                         FLIF16ColorVal* prevPlanes,
                                         FLIF16ColorVal* minv,
                                         FLIF16ColorVal* maxv)
{
    ranges_priv_channelcompact* data = r_ctx->priv_data;
    *minv = 0;
    *maxv = data->nb_colors[p];
}

static inline FLIF16ColorVal ff_ycocg_min(FLIF16ColorRangesContext* r_ctx, int p)
{   
    ranges_priv_ycocg* data = r_ctx->priv_data;
    FLIF16ColorRanges* ranges = flif16_ranges[data->r_ctx->r_no];
    switch(p) {
        case 0:
            return 0;
        case 1:
            return -4 * data->origmax4 + 1;
        case 2:
            return -4 * data->origmax4 + 1;
        default:
            return ranges->min(data->r_ctx, p);
    }
}

static inline FLIF16ColorVal ff_ycocg_max(FLIF16ColorRangesContext* r_ctx, int p)
{
    ranges_priv_ycocg* data = r_ctx->priv_data;
    FLIF16ColorRanges* ranges = flif16_ranges[data->r_ctx->r_no];
    switch(p) {
        case 0:
            return 4 * data->origmax4 - 1;
        case 1:
            return 4 * data->origmax4 - 1;
        case 2:
            return 4 * data->origmax4 - 1;
        default:
            return ranges->max(data->r_ctx, p);
    }
}

static inline void ff_ycocg_minmax(FLIF16ColorRangesContext *r_ctx ,const int p,
                                FLIF16ColorVal* prevPlanes,
                                FLIF16ColorVal* minv,
                                FLIF16ColorVal* maxv)
{
    ranges_priv_ycocg* data = r_ctx->priv_data;
    FLIF16ColorRanges* ranges = flif16_ranges[data->r_ctx->r_no];
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
            ranges->minmax(data->r_ctx, p, prevPlanes, minv, maxv);
    }
}

static inline FLIF16ColorVal ff_permuteplanessubtract_min(
                                            FLIF16ColorRangesContext* r_ctx,
                                            int p)
{
    ranges_priv_permuteplanes* data = r_ctx->priv_data;
    FLIF16ColorRanges* ranges = flif16_ranges[data->r_ctx->r_no];
    if(p==0 || p>2)
        return ranges->min(data->r_ctx, data->permutation[p]);
    return ranges->min(data->r_ctx, data->permutation[p]) - 
           ranges->max(data->r_ctx, data->permutation[0]);
}

static inline FLIF16ColorVal ff_permuteplanessubtract_max(
                                            FLIF16ColorRangesContext* r_ctx,
                                            int p)
{
    ranges_priv_permuteplanes* data = r_ctx->priv_data;
    FLIF16ColorRanges* ranges = flif16_ranges[data->r_ctx->r_no];
    if(p==0 || p>2)
        return ranges->max(data->r_ctx, data->permutation[p]);
    return ranges->max(data->r_ctx, data->permutation[p]) - 
           ranges->min(data->r_ctx, data->permutation[0]);
}

static inline FLIF16ColorVal ff_permuteplanes_min(
                                            FLIF16ColorRangesContext* r_ctx,
                                            int p)
{
    transform_priv_permuteplanes* data = r_ctx->priv_data;
    FLIF16ColorRanges* ranges = flif16_ranges[data->r_ctx->r_no];
    return ranges->min(data->r_ctx, data->permutation[p]);
}

static inline FLIF16ColorVal ff_permuteplanes_max(
                                            FLIF16ColorRangesContext* r_ctx,
                                            int p)
{
    transform_priv_permuteplanes* data = r_ctx->priv_data;
    FLIF16ColorRanges* ranges = flif16_ranges[data->r_ctx->r_no];
    return ranges->max(data->r_ctx, data->permutation[p]);
}

static inline void ff_permuteplanessubtract_minmax(
                                    FLIF16ColorRangesContext* r_ctx, int p,
                                    FLIF16ColorVal* prevPlanes, 
                                    FLIF16ColorVal* minv, FLIF16ColorVal* maxv)
{
    ranges_priv_permuteplanes* data = r_ctx->priv_data;
    FLIF16ColorRanges* ranges = flif16_ranges[data->r_ctx->r_no];
    if(p==0 || p>2){
        *minv = ranges->min(data->r_ctx, p);
        *maxv = ranges->max(data->r_ctx, p);
    }
    else{
        *minv = ranges->min(data->r_ctx, data->permutation[p]) - prevPlanes[0];
        *maxv = ranges->max(data->r_ctx, data->permutation[p]) - prevPlanes[0];
    }
}

static inline void ff_bounds_snap(FLIF16ColorRangesContext* r_ctx, 
                                  int p, FLIF16ColorVal* prevPlanes,
                                  FLIF16ColorVal* minv,
                                  FLIF16ColorVal* maxv,
                                  FLIF16ColorVal* v)
{
    ranges_priv_bounds *data = r_ctx->priv_data;
    FLIF16ColorRanges* ranges = flif16_ranges[data->r_ctx->r_no];
    if(p==0 || p==3){
        *minv = data->bounds[0][p];
        *maxv = data->bounds[1][p];
        return;
    }
    else{
        ranges->snap(data->r_ctx, p, prevPlanes, minv, maxv, v);
        if(*minv < data->bounds[0][p])
            *minv = data->bounds[0][p];
        if(*maxv > data->bounds[1][p])
            *maxv = data->bounds[1][p];

        if(*minv > *maxv){
            *minv = data->bounds[0][p];
            *maxv = data->bounds[1][p];
        }
        if(*v > *maxv)
            *v = *maxv;
        if(*v < *minv)
            *v = *minv;

    }
}

static inline void ff_bounds_minmax(FLIF16ColorRangesContext* r_ctx, 
                                    int p, FLIF16ColorVal* prevPlanes,
                                    FLIF16ColorVal* minv, FLIF16ColorVal* maxv)
{
    ranges_priv_bounds *data = r_ctx->priv_data;
    FLIF16ColorRanges* ranges = flif16_ranges[data->r_ctx->r_no];
    assert(p < r_ctx->num_planes);
    if(p==0 || p==3){
        *minv = data->bounds[0][p];
        *maxv = data->bounds[1][p];
        return;
    }
    ranges->minmax(data->r_ctx, p, prevPlanes, minv, maxv);
    if(*minv < data->bounds[0][p])
        *minv = data->bounds[0][p];
    if(*maxv > data->bounds[1][p])
        *maxv = data->bounds[1][p];

    if(*minv > *maxv){
        *minv = data->bounds[0][p];
        *maxv = data->bounds[1][p];
    }
    assert(*minv <= *maxv);
}

static inline FLIF16ColorVal ff_bounds_min(FLIF16ColorRangesContext* r_ctx,
                                           int p)
{
    FLIF16ColorRanges* ranges = flif16_ranges[r_ctx->r_no];
    ranges_priv_bounds* data = r_ctx->priv_data;
    assert(p < r_ctx->num_planes);
    return FFMAX(ranges->min(r_ctx, p), data->bounds[0][p]);
}

static inline FLIF16ColorVal ff_bounds_max(FLIF16ColorRangesContext* r_ctx,
                                           int p)
{
    FLIF16ColorRanges* ranges = flif16_ranges[r_ctx->r_no];
    ranges_priv_bounds* data = r_ctx->priv_data;
    assert(p < r_ctx->num_planes);
    return FFMIN(ranges->max(r_ctx, p), data->bounds[1][p]);
}

static inline void ff_ranges_close(FLIF16ColorRangesContext* r_ctx){
    FLIF16ColorRanges* ranges = flif16_ranges[r_ctx->r_no];
    if(ranges->priv_data_size)
        av_freep(r_ctx->priv_data);
    av_freep(r_ctx);
}

static inline void ff_transforms_close(FLIF16TransformContext* ctx){
    FLIF16Transform* trans = flif16_transforms[ctx->t_no];
    if(trans->priv_data_size)
        av_freep(ctx->priv_data);
    av_freep(ctx);
}

uint8_t ff_flif16_transform_read(FLIF16TransformContext*, 
                                FLIF16DecoderContext*,
                                FLIF16ColorRangesContext*);
FLIF16TransformContext *ff_flif16_transform_init(int, 
                                                 FLIF16ColorRangesContext*);

#endif /* FLIF16_TRANSFORM_H */
