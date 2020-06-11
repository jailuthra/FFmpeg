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

#include "avcodec.h"
#include "flif16.h"
#include "libavutil/common.h"

#define MAX_PLANES 5

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

void ff_default_snap(FLIF16ColorRangesContext*, int,
                                   FLIF16ColorVal*, FLIF16ColorVal*,
                                   FLIF16ColorVal*, FLIF16ColorVal*);
void ff_bounds_snap(FLIF16ColorRangesContext*, int,
                                   FLIF16ColorVal*, FLIF16ColorVal*,
                                   FLIF16ColorVal*, FLIF16ColorVal*);
void ff_default_minmax(FLIF16ColorRangesContext*, int,
                                     FLIF16ColorVal*, FLIF16ColorVal*,
                                     FLIF16ColorVal*);
void ff_channelcompact_minmax(FLIF16ColorRangesContext*, int,
                                            FLIF16ColorVal*, FLIF16ColorVal*,
                                            FLIF16ColorVal*);
void ff_ycocg_minmax(FLIF16ColorRangesContext*, int,
                                   FLIF16ColorVal*, FLIF16ColorVal*,
                                   FLIF16ColorVal*);
void ff_permuteplanessubtract_minmax(
                                            FLIF16ColorRangesContext*, int,
                                            FLIF16ColorVal*, FLIF16ColorVal*,
                                            FLIF16ColorVal*);
void ff_bounds_minmax(FLIF16ColorRangesContext*, int,
                                    FLIF16ColorVal*, FLIF16ColorVal*,
                                    FLIF16ColorVal*);
FLIF16ColorVal ff_static_min(FLIF16ColorRangesContext*, int);
FLIF16ColorVal ff_channelcompact_min(FLIF16ColorRangesContext*, int);
FLIF16ColorVal ff_ycocg_min(FLIF16ColorRangesContext*, int);
FLIF16ColorVal ff_permuteplanes_min(FLIF16ColorRangesContext*, int);
FLIF16ColorVal ff_permuteplanessubtract_min(FLIF16ColorRangesContext*, int);
FLIF16ColorVal ff_bounds_min(FLIF16ColorRangesContext*, int);

FLIF16ColorVal ff_static_max(FLIF16ColorRangesContext*, int);
FLIF16ColorVal ff_channelcompact_max(FLIF16ColorRangesContext*, int);
FLIF16ColorVal ff_ycocg_max(FLIF16ColorRangesContext*, int);
FLIF16ColorVal ff_permuteplanes_max(FLIF16ColorRangesContext*, int);
FLIF16ColorVal ff_permuteplanessubtract_max(FLIF16ColorRangesContext*, int);
FLIF16ColorVal ff_bounds_max(FLIF16ColorRangesContext*, int);


void ff_ranges_close(FLIF16ColorRangesContext*);
/*
FLIF16ColorRanges* ff_get_ranges(FLIF16InterimPixelData *pixel_data,
                                 FLIF16ColorRanges *ranges)
{
    int i, c, r, width, height;
    FLIF16ColorVal min, max;
    int p = pixel_data->ranges.num_planes;
    ranges->num_planes = p;
    width = pixel_data->width;
    height = pixel_data->height;
    for (i=0; i<p; i++) {
        min = pixel_data->data[p][0];
        max = pixel_data->data[p][0];
        for (r=0; r<height; r++) {
            for(c=0; c<width; c++) {
                if (min > pixel_data->data[p][r*width + c])
                    min = pixel_data->data[p][r*width + c];
                if(max < pixel_data->data[p][r*width + c])
                    max = pixel_data->data[p][r*width + c];
            }
        }
        ranges->min(p) = min;
        ranges->max(p) = max;
    }
    return ranges;
}
*/

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

uint8_t ff_flif16_transform_ycocg_init(FLIF16TransformContext*,
                                       FLIF16ColorRangesContext*);
FLIF16ColorRangesContext* ff_flif16_transform_ycocg_meta(
                                    FLIF16TransformContext*,
                                    FLIF16ColorRangesContext*);                                       
uint8_t ff_flif16_transform_ycocg_forward(FLIF16TransformContext*,
                                          FLIF16InterimPixelData*);
uint8_t ff_flif16_transform_ycocg_reverse(FLIF16TransformContext*,
                                          FLIF16InterimPixelData*,
                                          uint32_t, uint32_t);

uint8_t ff_flif16_transform_permuteplanes_read(FLIF16TransformContext*,
                                               FLIF16DecoderContext*,
                                               FLIF16ColorRangesContext*);
uint8_t ff_flif16_transform_permuteplanes_init(FLIF16TransformContext*,
                                               FLIF16ColorRangesContext*);
FLIF16ColorRangesContext* ff_flif16_transform_permuteplanes_meta(
                                    FLIF16TransformContext*,
                                    FLIF16ColorRangesContext*);
uint8_t ff_flif16_transform_permuteplanes_forward(
                                            FLIF16TransformContext*,
                                            FLIF16InterimPixelData*);
uint8_t ff_flif16_transform_permuteplanes_reverse(
                                        FLIF16TransformContext*,
                                        FLIF16InterimPixelData*,
                                        uint32_t, uint32_t);

uint8_t ff_flif16_transform_channelcompact_read(FLIF16TransformContext*,
                                               FLIF16DecoderContext*,
                                               FLIF16ColorRangesContext*);
uint8_t ff_flif16_transform_channelcompact_init(FLIF16TransformContext*,
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

FLIF16TransformContext *ff_flif16_transform_init(int, 
                                                 FLIF16ColorRangesContext*);
uint8_t ff_flif16_transform_read(FLIF16TransformContext*, 
                                 FLIF16DecoderContext*,
                                 FLIF16ColorRangesContext*);
void ff_transforms_close(FLIF16TransformContext*);

#endif /* FLIF16_TRANSFORM_H */
