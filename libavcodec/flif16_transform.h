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

typedef enum FLIF16RangesTypes{
    FLIF16_RANGES_CHANNELCOMPACT,
    FLIF16_RANGES_YCOCG,
    FLIF16_RANGES_PERMUTEPLANES,
    FLIF16_RANGES_PERMUTEPLANESSUBTRACT,
    FLIF16_RANGES_BOUNDS,
    FLIF16_RANGES_STATIC,
    FLIF16_RANGES_PALETTEALPHA,
    FLIF16_RANGES_PALETTE,
    FLIF16_RANGES_COLORBUCKETS,
    FLIF16_RANGES_DUPLICATEFRAME,
    FLIF16_RANGES_FRAMESHAPE,
    FLIF16_RANGES_FRAMELOOKBACK,
    FLIF16_RANGES_DUP
} FLIF16RangesTypes;

extern FLIF16Ranges *flif16_ranges[14];
extern FLIF16Transform *flif16_transforms[13];

FLIF16RangesContext *ff_flif16_ranges_static_init(unsigned int channels,
                                                  unsigned int bpc);

static inline void ff_flif16_ranges_close(FLIF16RangesContext* r_ctx){
    FLIF16Ranges* ranges = flif16_ranges[r_ctx->r_no];
    if(ranges->priv_data_size){
        ranges->close(r_ctx);
        av_free(r_ctx->priv_data);
    }
    av_freep(&r_ctx);
}

static inline FLIF16ColorVal ff_flif16_ranges_min(FLIF16RangesContext *r_ctx, int p)
{
    FLIF16Ranges *ranges = flif16_ranges[r_ctx->r_no];
    if(ranges->min)
        return ranges->min(r_ctx, p);
    else
        return 0;
}

static inline FLIF16ColorVal ff_flif16_ranges_max(FLIF16RangesContext *r_ctx, int p)
{
    FLIF16Ranges* ranges = flif16_ranges[r_ctx->r_no];
    if(ranges->max)
        return ranges->max(r_ctx, p);
    else
        return 0;
}

static inline void ff_flif16_ranges_minmax(FLIF16RangesContext *r_ctx, int p,
                                           FLIF16ColorVal *prev_planes, 
                                           FLIF16ColorVal *minv, FLIF16ColorVal *maxv)
{
    flif16_ranges[r_ctx->r_no]->minmax(r_ctx, p, prev_planes, minv, maxv);
}

static inline void ff_flif16_ranges_snap(FLIF16RangesContext *r_ctx, int p,
                                         FLIF16ColorVal *prev_planes, FLIF16ColorVal *minv, 
                                         FLIF16ColorVal *maxv, FLIF16ColorVal *v)
{
    flif16_ranges[r_ctx->r_no]->snap(r_ctx, p, prev_planes, minv, maxv, v);
}

FLIF16TransformContext *ff_flif16_transform_init(int, FLIF16RangesContext *);

int8_t ff_flif16_transform_read(FLIF16TransformContext *, FLIF16DecoderContext *,
                                 FLIF16RangesContext *);

FLIF16RangesContext* ff_flif16_transform_meta(FLIF16PixelData *,
                                             uint32_t,
                                             FLIF16TransformContext *, 
                                             FLIF16RangesContext *);

int8_t ff_flif16_transform_reverse(FLIF16TransformContext*, FLIF16PixelData*,
                                    uint8_t, uint8_t);

void ff_flif16_transforms_close(FLIF16TransformContext *);

#endif /* FLIF16_TRANSFORM_H */
