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

#include "flif16_transform.h"
#include "flif16_rangecoder.h"
#include "libavutil/common.h"

FLIF16Ranges flif16_colorranges_default = {
    .priv_data_size = 0,
    .min            = NULL,
    .max            = NULL,
    .minmax         = &ff_default_minmax,
    .snap           = &ff_default_snap,
    .is_static      = 1,
};

FLIF16Ranges flif16_colorranges_static = {
    .priv_data_size = sizeof(ranges_priv_static),
    .min            = &ff_static_min,
    .max            = &ff_static_max,
    .minmax         = &ff_default_minmax,
    .snap           = &ff_default_snap,
    .is_static      = 1,
};

FLIF16Ranges flif16_colorranges_channelcompact = {
    .priv_data_size = sizeof(ranges_priv_channelcompact),
    .min            = &ff_channelcompact_min,
    .max            = &ff_channelcompact_max,
    .minmax         = &ff_channelcompact_minmax,
    .snap           = &ff_default_snap,
    .is_static      = 1,
};

FLIF16Ranges flif16_colorranges_ycocg = {
    .priv_data_size = sizeof(ranges_priv_ycocg),
    .min            = &ff_ycocg_min,
    .max            = &ff_ycocg_max,
    .minmax         = &ff_ycocg_minmax,
    .snap           = &ff_default_snap,
    .is_static      = 0,
};

FLIF16Ranges flif16_colorranges_permuteplanessubtract = {
    .priv_data_size = sizeof(ranges_priv_permuteplanes),
    .min            = &ff_permuteplanessubtract_min,
    .max            = &ff_permuteplanessubtract_max,
    .minmax         = &ff_permuteplanessubtract_minmax,
    .snap           = &ff_default_snap,
    .is_static      = 0,
};

FLIF16Ranges flif16_colorranges_permuteplanes = {
    .priv_data_size = sizeof(ranges_priv_permuteplanes),
    .min    = &ff_permuteplanes_min,
    .max    = &ff_permuteplanes_max,
    .minmax = &ff_default_minmax,
    .snap   = &ff_default_snap,
    .is_static = 0,
};

FLIF16Ranges flif16_colorranges_bounds = {
    .priv_data_size = sizeof(ranges_priv_bounds),
    .min    = &ff_bounds_min,
    .max    = &ff_bounds_max,
    .minmax = &ff_bounds_minmax,
    .snap   = &ff_bounds_snap,
    .is_static = 0,
};

/*
FLIF16Ranges* flif16_ranges[] = {
    &flif16_colorranges_default,
    &flif16_colorranges_channelcompact,
    &flif16_colorranges_ycocg,
    NULL, // FLIF16_TRANSFORM_RESERVED1
    &flif16_colorranges_permuteplanes,
    &flif16_colorranges_permuteplanessubtract,
    &flif16_colorranges_bounds,
    &flif16_colorranges_static
};
*/

FLIF16Ranges *flif16_ranges[] = {
    &flif16_colorranges_channelcompact, // FLIF16_TRANSFORM_CHANNELCOMPACT = 0,
    &flif16_colorranges_ycocg,          // FLIF16_TRANSFORM_YCOCG,
    NULL, // FLIF16_TRANSFORM_RESERVED1,
    &flif16_colorranges_permuteplanes,  // FLIF16_TRANSFORM_PERMUTEPLANES,
    &flif16_colorranges_bounds,         // FLIF16_TRANSFORM_BOUNDS,
    NULL, // FLIF16_TRANSFORM_PALETTEALPHA,
    NULL, // FLIF16_TRANSFORM_PALETTE,
    NULL, // FLIF16_TRANSFORM_COLORBUCKETS,
    NULL, // FLIF16_TRANSFORM_RESERVED2,
    NULL, // FLIF16_TRANSFORM_RESERVED3,
    NULL, // FLIF16_TRANSFORM_DUPLICATEFRAME,
    NULL, // FLIF16_TRANSFORM_FRAMESHAPE,
    NULL, // FLIF16_TRANSFORM_FRAMELOOKBACK
};

void ff_default_minmax(FLIF16RangesContext *src_ctx ,const int p,
                       FLIF16ColorVal* prev_planes,
                       FLIF16ColorVal* minv, FLIF16ColorVal* maxv)
{
    FLIF16Ranges* ranges = flif16_ranges[src_ctx->r_no];
    *minv = ranges->min(src_ctx, p);
    *maxv = ranges->max(src_ctx, p);
}

void ff_default_snap(FLIF16RangesContext *src_ctx ,const int p,
                     FLIF16ColorVal* prev_planes,
                     FLIF16ColorVal* minv, FLIF16ColorVal* maxv, 
                     FLIF16ColorVal* v)
{
    ff_default_minmax(src_ctx, p, prev_planes, minv, maxv);
    if(*minv > *maxv)
        *maxv = *minv;
    *v = av_clip(*v, *minv, *maxv);
}

FLIF16ColorVal ff_static_min(FLIF16RangesContext* r_ctx,
                             int p)
{
    ranges_priv_static* data = r_ctx->priv_data;
    if(p >= r_ctx->num_planes)
        return 0;
    assert(p < r_ctx->num_planes);
    return data->bounds[0][p];
}

FLIF16ColorVal ff_static_max(FLIF16RangesContext* r_ctx,
                             int p)
{
    ranges_priv_static* data = r_ctx->priv_data;
    if(p >= r_ctx->num_planes)
        return 0;
    assert(p < r_ctx->num_planes);
    return data->bounds[1][p];
}

FLIF16ColorVal ff_channelcompact_min(FLIF16RangesContext* ranges, 
                                     int p){
    return 0;
}

FLIF16ColorVal ff_channelcompact_max(FLIF16RangesContext* src_ctx,
                                     int p){
    ranges_priv_channelcompact* data = src_ctx->priv_data;
    return data->nb_colors[p];
}

void ff_channelcompact_minmax(FLIF16RangesContext *r_ctx, int p,
                              FLIF16ColorVal* prev_planes,
                              FLIF16ColorVal* minv,
                              FLIF16ColorVal* maxv)
{
    ranges_priv_channelcompact* data = r_ctx->priv_data;
    *minv = 0;
    *maxv = data->nb_colors[p];
}

FLIF16ColorVal ff_ycocg_min(FLIF16RangesContext* r_ctx, int p)
{   
    ranges_priv_ycocg* data = r_ctx->priv_data;
    FLIF16Ranges* ranges = flif16_ranges[data->r_ctx->r_no];
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

FLIF16ColorVal ff_ycocg_max(FLIF16RangesContext* r_ctx, int p)
{
    ranges_priv_ycocg* data = r_ctx->priv_data;
    FLIF16Ranges* ranges = flif16_ranges[data->r_ctx->r_no];
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

void ff_ycocg_minmax(FLIF16RangesContext *r_ctx ,const int p,
                     FLIF16ColorVal* prev_planes,
                     FLIF16ColorVal* minv,
                     FLIF16ColorVal* maxv)
{
    ranges_priv_ycocg* data = r_ctx->priv_data;
    FLIF16Ranges* ranges = flif16_ranges[data->r_ctx->r_no];
    switch(p){
        case 0:
            *minv = 0;
            *maxv = ff_get_max_y(data->origmax4);
            break;
        case 1:
            *minv = ff_get_min_co(data->origmax4, prev_planes[0]);
            *maxv = ff_get_max_co(data->origmax4, prev_planes[0]);
            break;    
        case 2:
            *minv = ff_get_min_cg( data->origmax4, prev_planes[0], prev_planes[1]);
            *maxv = ff_get_max_cg( data->origmax4, prev_planes[0], prev_planes[1]);
            break;
        default:
            ranges->minmax(data->r_ctx, p, prev_planes, minv, maxv);
    }
}

FLIF16ColorVal ff_permuteplanessubtract_min(FLIF16RangesContext* r_ctx,
                                            int p)
{
    ranges_priv_permuteplanes* data = r_ctx->priv_data;
    FLIF16Ranges* ranges = flif16_ranges[data->r_ctx->r_no];
    if(p==0 || p>2)
        return ranges->min(data->r_ctx, data->permutation[p]);
    return ranges->min(data->r_ctx, data->permutation[p]) - 
           ranges->max(data->r_ctx, data->permutation[0]);
}

FLIF16ColorVal ff_permuteplanessubtract_max(FLIF16RangesContext* r_ctx,
                                            int p)
{
    ranges_priv_permuteplanes* data = r_ctx->priv_data;
    FLIF16Ranges* ranges = flif16_ranges[data->r_ctx->r_no];
    if(p==0 || p>2)
        return ranges->max(data->r_ctx, data->permutation[p]);
    return ranges->max(data->r_ctx, data->permutation[p]) - 
           ranges->min(data->r_ctx, data->permutation[0]);
}

FLIF16ColorVal ff_permuteplanes_min(FLIF16RangesContext* r_ctx,
                                    int p)
{
    transform_priv_permuteplanes* data = r_ctx->priv_data;
    FLIF16Ranges* ranges = flif16_ranges[data->r_ctx->r_no];
    return ranges->min(data->r_ctx, data->permutation[p]);
}

FLIF16ColorVal ff_permuteplanes_max(FLIF16RangesContext* r_ctx,
                                    int p)
{
    transform_priv_permuteplanes* data = r_ctx->priv_data;
    FLIF16Ranges* ranges = flif16_ranges[data->r_ctx->r_no];
    return ranges->max(data->r_ctx, data->permutation[p]);
}

void ff_permuteplanessubtract_minmax(FLIF16RangesContext* r_ctx, int p,
                                     FLIF16ColorVal* prev_planes, 
                                     FLIF16ColorVal* minv, FLIF16ColorVal* maxv)
{
    ranges_priv_permuteplanes* data = r_ctx->priv_data;
    FLIF16Ranges* ranges = flif16_ranges[data->r_ctx->r_no];
    if(p==0 || p>2){
        *minv = ranges->min(data->r_ctx, p);
        *maxv = ranges->max(data->r_ctx, p);
    }
    else{
        *minv = ranges->min(data->r_ctx, data->permutation[p]) - prev_planes[0];
        *maxv = ranges->max(data->r_ctx, data->permutation[p]) - prev_planes[0];
    }
}

void ff_bounds_snap(FLIF16RangesContext* r_ctx, 
                    int p, FLIF16ColorVal* prev_planes,
                    FLIF16ColorVal* minv,
                    FLIF16ColorVal* maxv,
                    FLIF16ColorVal* v)
{
    ranges_priv_bounds *data = r_ctx->priv_data;
    FLIF16Ranges* ranges = flif16_ranges[data->r_ctx->r_no];
    if(p==0 || p==3){
        *minv = data->bounds[0][p];
        *maxv = data->bounds[1][p];
        return;
    }
    else{
        ranges->snap(data->r_ctx, p, prev_planes, minv, maxv, v);
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

void ff_bounds_minmax(FLIF16RangesContext* r_ctx, 
                      int p, FLIF16ColorVal* prev_planes,
                      FLIF16ColorVal* minv, FLIF16ColorVal* maxv)
{
    ranges_priv_bounds *data = r_ctx->priv_data;
    FLIF16Ranges* ranges = flif16_ranges[data->r_ctx->r_no];
    assert(p < r_ctx->num_planes);
    if(p==0 || p==3){
        *minv = data->bounds[0][p];
        *maxv = data->bounds[1][p];
        return;
    }
    ranges->minmax(data->r_ctx, p, prev_planes, minv, maxv);
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

FLIF16ColorVal ff_bounds_min(FLIF16RangesContext* r_ctx,
                             int p)
{
    FLIF16Ranges* ranges = flif16_ranges[r_ctx->r_no];
    ranges_priv_bounds* data = r_ctx->priv_data;
    assert(p < r_ctx->num_planes);
    return FFMAX(ranges->min(r_ctx, p), data->bounds[0][p]);
}

FLIF16ColorVal ff_bounds_max(FLIF16RangesContext* r_ctx,
                             int p)
{
    FLIF16Ranges* ranges = flif16_ranges[r_ctx->r_no];
    ranges_priv_bounds* data = r_ctx->priv_data;
    assert(p < r_ctx->num_planes);
    return FFMIN(ranges->max(r_ctx, p), data->bounds[1][p]);
}

void ff_ranges_close(FLIF16RangesContext* r_ctx){
    FLIF16Ranges* ranges = flif16_ranges[r_ctx->r_no];
    if(ranges->priv_data_size)
        av_freep(r_ctx->priv_data);
    av_freep(r_ctx);
}

uint8_t ff_flif16_transform_ycocg_init(FLIF16TransformContext *ctx, 
                                       FLIF16RangesContext* r_ctx)
{   
    transform_priv_ycocg *data = ctx->priv_data;
    FLIF16Ranges* src_ranges = flif16_ranges[r_ctx->r_no];
    if(r_ctx->num_planes < 3) 
        return 0;
    
    if(   src_ranges->min(r_ctx, 0) == src_ranges->max(r_ctx, 0) 
       || src_ranges->min(r_ctx, 1) == src_ranges->max(r_ctx, 1) 
       || src_ranges->min(r_ctx, 2) == src_ranges->max(r_ctx, 2))
        return 0;
    
    if(  src_ranges->min(r_ctx, 0) < 1 
       ||src_ranges->min(r_ctx, 1) < 1 
       ||src_ranges->min(r_ctx, 2) < 1) 
        return 0;

    data->origmax4 = FFMAX3(src_ranges->max(r_ctx, 0), 
                            src_ranges->max(r_ctx, 1), 
                            src_ranges->max(r_ctx, 2))/4 -1;

    data->r_ctx = r_ctx;
    return 1;
}

FLIF16RangesContext* ff_flif16_transform_ycocg_meta(FLIF16TransformContext* ctx,
                                                    FLIF16RangesContext* src_ctx)
{
    FLIF16RangesContext* r_ctx;
    ranges_priv_ycocg* data;
    transform_priv_ycocg* trans_data = ctx->priv_data;
    r_ctx = av_mallocz(sizeof(FLIF16Ranges));
    r_ctx->r_no = FLIF16_RANGES_YCOCG;
    r_ctx->priv_data = av_mallocz(sizeof(ranges_priv_ycocg));
    data = r_ctx->priv_data;
    
    //Here the ranges_priv_ycocg contents are being copied.
    data->origmax4 = trans_data->origmax4;
    data->r_ctx    = trans_data->r_ctx;
    
    r_ctx->num_planes = src_ctx->num_planes;

    return r_ctx;
}

uint8_t ff_flif16_transform_ycocg_forward(FLIF16TransformContext* ctx,
                                          FLIF16InterimPixelData* pixel_data)
{
    int r, c;
    FLIF16ColorVal R,G,B,Y,Co,Cg;

    int height = pixel_data->height;
    int width = pixel_data->width;

    for (r=0; r<height; r++) {
        for (c=0; c<width; c++) {
            R = pixel_data->data[0][r*width + c];
            G = pixel_data->data[1][r*width + c];
            B = pixel_data->data[2][r*width + c];

            Y = (((R + B)>>1) + G)>>1;
            Co = R - B;
            Cg = G - ((R + B)>>1);

            pixel_data->data[0][r*width + c] = Y;
            pixel_data->data[1][r*width + c] = Co;
            pixel_data->data[2][r*width + c] = Cg;
        }
    }
    return 1;
}

uint8_t ff_flif16_transform_ycocg_reverse(FLIF16TransformContext *ctx,
                                          FLIF16InterimPixelData * pixel_data,
                                          uint32_t stride_row,
                                          uint32_t stride_col)
{
    int r, c;
    FLIF16ColorVal R,G,B,Y,Co,Cg;
    int height = pixel_data->height;
    int width  = pixel_data->width;
    transform_priv_ycocg *data = ctx->priv_data;
    FLIF16Ranges* ranges = flif16_ranges[data->r_ctx->r_no];

    for (r=0; r<height; r+=stride_row) {
        for (c=0; c<width; c+=stride_col) {
            Y  = pixel_data->data[0][r*width + c];
            Co = pixel_data->data[1][r*width + c];
            Cg = pixel_data->data[2][r*width + c];
  
            R = Co + Y + ((1-Cg)>>1) - (Co>>1);
            G = Y - ((-Cg)>>1);
            B = Y + ((1-Cg)>>1) - (Co>>1);

            R = av_clip(R, 0, ranges->max(data->r_ctx, 0));
            G = av_clip(G, 0, ranges->max(data->r_ctx, 1));
            B = av_clip(B, 0, ranges->max(data->r_ctx, 2));

            pixel_data->data[0][r*width + c] = R;
            pixel_data->data[1][r*width + c] = G;
            pixel_data->data[2][r*width + c] = B;
        }
    }
    return 0;
}

uint8_t ff_flif16_transform_permuteplanes_init(FLIF16TransformContext* ctx, 
                                               FLIF16RangesContext* r_ctx)
{
    transform_priv_permuteplanes *data = ctx->priv_data;
    FLIF16Ranges* ranges = flif16_ranges[r_ctx->r_no];
    data->ctx_a = ff_flif16_chancecontext_init();
    
    if(r_ctx->num_planes< 3)
        return 0;
    if( ranges->min(r_ctx, 0) < 0
      ||ranges->min(r_ctx, 1) < 0
      ||ranges->min(r_ctx, 2) < 0) 
        return 0;
    
    data->r_ctx = r_ctx;
    return 1;
}

uint8_t ff_flif16_transform_permuteplanes_read(FLIF16TransformContext* ctx,
                                               FLIF16DecoderContext* dec_ctx,
                                               FLIF16RangesContext* r_ctx)
{
    int p;
    transform_priv_permuteplanes* data = ctx->priv_data;

    switch (ctx->segment) {
        case 0:
            RAC_GET(dec_ctx->rc, data->ctx_a, 0, 1, &data->subtract,
                    FLIF16_RAC_NZ_INT);
            //data->subtract = read_nz_int(rac, 0, 1);
            ++ctx->segment; __PLN__
            
            for(p=0; p<4; p++){
                data->from[p] = 0;
                data->to[p] = 0;
            }
        case 1:
            for (; ctx->i < dec_ctx->channels; ++ctx->i) {
                RAC_GET(dec_ctx->rc, data->ctx_a, 0, dec_ctx->channels-1,
                        &data->permutation[ctx->i], 
                        FLIF16_RAC_NZ_INT);
                //data->permutation[p] = read_nz_int(s->rc, 0, s->channels-1);
                data->from[ctx->i] = 1;
                data->to[ctx->i] = 1;
            }
            ctx->i = 0;

            for (p = 0; p < dec_ctx->channels; p++) {
                if(!data->from[p] || !data->to[p])
                return 0;
            }
            ++ctx->segment; __PLN__
            goto end;
    }

    end:
        ctx->segment = 0;
        return 1;

    need_more_data:
        return AVERROR(EAGAIN);
}

FLIF16RangesContext* ff_flif16_transform_permuteplanes_meta(FLIF16TransformContext* ctx,
                                                            FLIF16RangesContext* src_ctx)
{
    FLIF16RangesContext* r_ctx = av_mallocz(sizeof(FLIF16Ranges));
    transform_priv_permuteplanes* data = ctx->priv_data;
    ranges_priv_permuteplanes* priv_data = av_mallocz(sizeof(ranges_priv_permuteplanes));
    int i;
    if(data->subtract)
        r_ctx->r_no = FLIF16_RANGES_PERMUTEPLANESSUBTRACT;
    else
        r_ctx->r_no = FLIF16_RANGES_PERMUTEPLANES;
    r_ctx->num_planes = src_ctx->num_planes;
    for(i=0; i<5; i++){
        priv_data->permutation[i] = data->permutation[i];
    }
    priv_data->r_ctx       = data->r_ctx;
    r_ctx->priv_data = priv_data;
    return r_ctx;
}

uint8_t ff_flif16_transform_permuteplanes_forward(FLIF16TransformContext* ctx,
                                                  FLIF16InterimPixelData* pixel_data)
{
    FLIF16ColorVal pixel[5];
    int r, c, p;
    int width  = pixel_data->width;
    int height = pixel_data->height;
    transform_priv_permuteplanes *data = ctx->priv_data;
    
    // Transforming pixel data.
    for (r=0; r<height; r++) {
        for (c=0; c<width; c++) {
            for (p=0; p<data->r_ctx->num_planes; p++)
                pixel[p] = pixel_data->data[p][r*width + c];
            pixel_data->data[0][r*width + c] = pixel[data->permutation[0]];
            if (!data->subtract){
                for (p=1; p<data->r_ctx->num_planes; p++)
                    pixel_data->data[p][r*width + c] = pixel[data->permutation[p]];
            }
            else{ 
                for(p=1; p<3 && p<data->r_ctx->num_planes; p++)
                    pixel_data->data[p][r*width + c] = 
                    pixel[data->permutation[p]] - pixel[data->permutation[0]];
                for(p=3; p<data->r_ctx->num_planes; p++)
                    pixel_data->data[p][r*width + c] = pixel[data->permutation[p]];
            }
        }
    }
    return 1;
}

uint8_t ff_flif16_transform_permuteplanes_reverse(FLIF16TransformContext *ctx,
                                                  FLIF16InterimPixelData * pixels,
                                                  uint32_t stride_row,
                                                  uint32_t stride_col)
{   
    int p, r, c;
    FLIF16ColorVal pixel[5];
    transform_priv_permuteplanes *data = ctx->priv_data;
    FLIF16Ranges* ranges = flif16_ranges[data->r_ctx->r_no];
    int height = pixels->height;
    int width  = pixels->width;
    for (r=0; r<height; r+=stride_row) {
        for (c=0; c<width; c+=stride_col) {
            for (p=0; p<data->r_ctx->num_planes; p++)
                pixel[p] = pixels->data[p][r*width + c];
            for (p=0; p<data->r_ctx->num_planes; p++)
                pixels->data[data->permutation[p]][r*width + c] = pixel[p];
            
            pixels->data[data->permutation[0]][r*width + c] = pixel[0];
            if (!data->subtract) {
                for (p=1; p<data->r_ctx->num_planes; p++)
                    pixels->data[data->permutation[p]][r*width + c] = pixel[p];
            } else {
                for (p=1; p<3 && p<data->r_ctx->num_planes; p++)
                    pixels->data[data->permutation[p]][r*width + c] =
                    av_clip(pixel[p] + pixel[0],
                         ranges->min(data->r_ctx, data->permutation[p]),
                         ranges->max(data->r_ctx, data->permutation[p]));
                for (p=3; p<data->r_ctx->num_planes; p++)
                    pixels->data[data->permutation[p]][r*width + c] = pixel[p];
            }
        }
    }
    return 1;
}

uint8_t ff_flif16_transform_channelcompact_read(FLIF16TransformContext * ctx,
                                                FLIF16DecoderContext *dec_ctx,
                                                FLIF16RangesContext* src_ctx)
{
    unsigned int nb;
    int remaining;
    transform_priv_channelcompact *data = ctx->priv_data;
    FLIF16Ranges* ranges = flif16_ranges[src_ctx->r_no];
    start:
    switch (ctx->segment) {
        case 0:
            if(ctx->i < dec_ctx->channels) {
                RAC_GET(dec_ctx->rc, data->ctx_a,
                        0, ranges->max(src_ctx, ctx->i) -
                        ranges->min(src_ctx, ctx->i),
                        &nb, FLIF16_RAC_NZ_INT);
                nb += 1;
                data->min = ranges->min(src_ctx, ctx->i);
                data->CPalette[ctx->i] = av_mallocz(nb*sizeof(FLIF16ColorVal));
                data->CPalette_size[ctx->i] = nb;
                remaining = nb-1;
                ++ctx->segment;
                goto next_case;
            }
            ctx->i = 0;
            goto end;
        
        next_case:
        case 1:
            for (; data->i < nb; ++data->i) {
                RAC_GET(dec_ctx->rc, data->ctx_a,
                        0, ranges->max(src_ctx, ctx->i)-data->min-remaining,
                        &data->CPalette[ctx->i][data->i], 
                        FLIF16_RAC_NZ_INT);
                data->CPalette[ctx->i][data->i] += data->min;
                //Basically I want to perform this operation :
                //CPalette[p][i] = min + read_nz_int(0, 255-min-remaining);
                data->min = data->CPalette[ctx->i][data->i]+1;
                remaining--;
            }
            data->i = 0;
            ctx->segment--;
            ctx->i++;
            goto start;
    }
    
    end:
        ctx->segment = 0;
        return 1;

    need_more_data:
        return AVERROR(EAGAIN);
}

uint8_t ff_flif16_transform_channelcompact_init(FLIF16TransformContext *ctx, 
                                                FLIF16RangesContext* src_ctx)
{
    int p;
    transform_priv_channelcompact *data = ctx->priv_data;
    if(src_ctx->num_planes > 4)
        return 0;
    
    for(p=0; p<4; p++){
        data->CPalette[p]       = 0;
        data->CPalette_size[p]  = 0;
    }    
    data->ctx_a = ff_flif16_chancecontext_init();
    return 1;
}

FLIF16RangesContext* ff_flif16_transform_channelcompact_meta(FLIF16TransformContext* ctx,
                                                             FLIF16RangesContext* src_ctx)
{
    int i;
    FLIF16RangesContext* r_ctx = av_mallocz(sizeof(FLIF16Ranges));
    ranges_priv_channelcompact* data = av_mallocz(sizeof(ranges_priv_channelcompact));
    transform_priv_channelcompact* trans_data = ctx->priv_data;
    r_ctx->num_planes = src_ctx->num_planes;
    for(i=0; i<src_ctx->num_planes; i++){
        data->nb_colors[i] = trans_data->CPalette_size[i] - 1;
    }
    r_ctx->priv_data = data;
    r_ctx->r_no = FLIF16_RANGES_CHANNELCOMPACT;
    return r_ctx;
}

uint8_t ff_flif16_transform_channelcompact_reverse(FLIF16TransformContext* ctx,
                                                   FLIF16InterimPixelData* pixels,
                                                   uint32_t stride_row,
                                                   uint32_t stride_col)
{   
    int p, P;
    uint32_t r, c;
    FLIF16ColorVal* palette;
    unsigned int palette_size;
    transform_priv_channelcompact *data = ctx->priv_data;
    
    for(p=0; p<pixels->num_planes; p++){
        palette      = data->CPalette[p];
        palette_size = data->CPalette_size[p];

        for(r=0; r < pixels->height; r++){
            for(c=0; c < pixels->width; c++){
                P = pixels->data[p][r*pixels->width + c];
                if (P < 0 || P >= (int) palette_size)
                    P = 0;
                assert(P < (int) palette_size);
                pixels->data[p][r*pixels->width + c] = palette[P];
            }
        }
    }
    return 1;
}

uint8_t ff_flif16_transform_bounds_init(FLIF16TransformContext *ctx, 
                                        FLIF16RangesContext* src_ctx)
{
    transform_priv_bounds *data = ctx->priv_data;
    if(src_ctx->num_planes > 4)
        return 0;

    data->ctx_a = ff_flif16_chancecontext_init();
    data->bounds[0] = av_mallocz(src_ctx->num_planes*sizeof(FLIF16ColorVal));
    data->bounds[1] = av_mallocz(src_ctx->num_planes*sizeof(FLIF16ColorVal));
    return 1;
}

uint8_t ff_flif16_transform_bounds_read(FLIF16TransformContext* ctx,
                                        FLIF16DecoderContext* dec_ctx,
                                        FLIF16RangesContext* src_ctx)
{
    transform_priv_bounds *data = ctx->priv_data;
    FLIF16Ranges* ranges = flif16_ranges[src_ctx->r_no];
    FLIF16ColorVal max;

    start:
    if(ctx->i < dec_ctx->channels){
        switch(ctx->segment){
            case 0:
                RAC_GET(dec_ctx->rc, data->ctx_a,
                        ranges->min(src_ctx, ctx->i), 
                        ranges->max(src_ctx, ctx->i),
                        &data->min, FLIF16_RAC_GNZ_INT);
                ctx->segment++;
        
            case 1:
                RAC_GET(dec_ctx->rc, data->ctx_a,
                        data->min, ranges->max(src_ctx, ctx->i),
                        &max, FLIF16_RAC_GNZ_INT);
                if(data->min > max)
                    return 0;
                if(data->min < ranges->min(src_ctx, ctx->i))
                    return 0;
                if(max > ranges->max(src_ctx, ctx->i))
                    return 0;
                data->bounds[0][ctx->i] = data->min;
                data->bounds[1][ctx->i] = max;
                ctx->i++;
                ctx->segment--;
                goto start;
        }
    }
    else{
        ctx->i = 0;
        ctx->segment = 0;
        goto end;
    }
    end:
        return 1;

    need_more_data:
        return AVERROR(EAGAIN);
}

FLIF16RangesContext* ff_flif16_transform_bounds_meta(FLIF16TransformContext* ctx,
                                                     FLIF16RangesContext* src_ctx)
{
    FLIF16RangesContext* r_ctx = av_mallocz(sizeof(FLIF16Ranges));
    transform_priv_bounds* trans_data = ctx->priv_data;
    ranges_priv_static* data;
    ranges_priv_bounds* dataB;
    r_ctx->num_planes = src_ctx->num_planes;
    
    if(flif16_ranges[src_ctx->r_no]->is_static){
        r_ctx->r_no = FLIF16_RANGES_STATIC;
        r_ctx->priv_data = av_mallocz(sizeof(ranges_priv_static));
        data = r_ctx->priv_data;
        data->bounds[0] = trans_data->bounds[0];
        data->bounds[1] = trans_data->bounds[1];
    }
    else{
        r_ctx->r_no = FLIF16_RANGES_BOUNDS;
        r_ctx->priv_data = av_mallocz(sizeof(ranges_priv_bounds));
        dataB = r_ctx->priv_data;
        dataB->bounds[0] = trans_data->bounds[0];
        dataB->bounds[1] = trans_data->bounds[1];
        dataB->r_ctx = src_ctx;
    }
    return r_ctx;
}

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

FLIF16TransformContext* ff_flif16_transform_init(int t_no, 
                                                 FLIF16RangesContext* r_ctx)
{
    FLIF16Transform *trans = flif16_transforms[t_no];
    FLIF16TransformContext *ctx = av_mallocz(sizeof(FLIF16TransformContext));
    void *k = NULL;
    if(!ctx)
        return NULL;
    if (trans->priv_data_size)
        k = av_mallocz(trans->priv_data_size);
    ctx->priv_data = k;
    ctx->segment = 0;
    ctx->i = 0;

    if (trans->init) {
        if(!trans->init(ctx, r_ctx))
            return NULL;
    }
    
    return ctx;
}


uint8_t ff_flif16_transform_read(FLIF16TransformContext *ctx,
                                 FLIF16DecoderContext *dec_ctx,
                                 FLIF16RangesContext* r_ctx)
{
    FLIF16Transform *trans = flif16_transforms[ctx->t_no];
    if(!ctx)
        return 0;
    if(trans->read)
        return trans->read(ctx, dec_ctx, r_ctx);
    else
        return 1;
}

void ff_transforms_close(FLIF16TransformContext* ctx)
{
    FLIF16Transform* trans = flif16_transforms[ctx->t_no];
    if(trans->priv_data_size)
        av_freep(ctx->priv_data);
    av_freep(ctx);
}
