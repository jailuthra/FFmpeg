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

FLIF16ColorRanges* ff_get_ranges( FLIF16InterimPixelData *pixel_data,
                                        FLIF16ColorRanges *ranges){
    int i, c, r, width, height;
    int p = pixel_data->ranges.num_planes;
    ranges->num_planes = p;
    FLIF16ColorVal min, max;
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
        ranges->min[p] = min;
        ranges->max[p] = max;
    }
    return ranges;
}

uint8_t ff_flif16_transform_ycocg_init(FLIF16TransformContext *ctx, 
                                       FLIF16DecoderContext *dec_ctx)
{   
    transform_priv_ycocg *data = ctx->priv_data;
    data->ranges.num_planes = dec_ctx->channels;
    if(dec_ctx->src_ranges.num_planes < 3) 
        return 0;
    
    if(   dec_ctx->src_ranges.min[0] == dec_ctx->src_ranges.max[0] 
       || dec_ctx->src_ranges.min[1] == dec_ctx->src_ranges.max[1] 
       || dec_ctx->src_ranges.min[2] == dec_ctx->src_ranges.max[2])
        return 0;
    
    if(  dec_ctx->src_ranges.min[0] < 1 
       ||dec_ctx->src_ranges.min[1] < 1 
       ||dec_ctx->src_ranges.min[2] < 1) 
        return 0;

    data->origmax4 = FFMAX3(dec_ctx->src_ranges.max[0], 
                            dec_ctx->src_ranges.max[1], 
                            dec_ctx->src_ranges.max[2])/4 -1;

    int p;
    for (p = 0; p < data->ranges.num_planes; p++) {
        data->ranges.max[p] = dec_ctx->src_ranges.max[p];
        data->ranges.min[p] = dec_ctx->src_ranges.min[p];
    }
    return 1;
}

uint8_t ff_flif16_transform_ycocg_forward(FLIF16TransformContext *ctx,
                                          FLIF16DecoderContext *dec_ctx,
                                          FLIF16InterimPixelData * pixel_data)
{
    transform_priv_ycocg *data = ctx->priv_data;
    int r, c;
    FLIF16ColorVal R,G,B,Y,Co,Cg;

    int height = dec_ctx->height;
    int width = dec_ctx->width;

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

    // Will see if ranges need to be stored separately in transform struct only.
    int p;
    for (p = 0; p < data->ranges.num_planes; p++) {
        data->ranges.max[p] = ff_max_range_ycocg(p, data->origmax4);
        data->ranges.min[p] = ff_min_range_ycocg(p, data->origmax4);
    }
    // I don't know yet what to do with this flag in forward
    // and reverse transforms.
    ctx->done = 1;
    return 1;
}

uint8_t ff_flif16_transform_ycocg_reverse(FLIF16TransformContext *ctx,
                                          FLIF16DecoderContext *dec_ctx,
                                          FLIF16InterimPixelData * pixel_data,
                                          uint32_t stride_row,
                                          uint32_t stride_col)
{
    transform_priv_ycocg *data = ctx->priv_data;
    int r, c;
    FLIF16ColorVal R,G,B,Y,Co,Cg;
    int height = dec_ctx->height;
    int width  = dec_ctx->width;

    for (r=0; r<height; r+=stride_row) {
        for (c=0; c<width; c+=stride_col) {
            Y  = pixel_data->data[0][r*width + c];
            Co = pixel_data->data[1][r*width + c];
            Cg = pixel_data->data[2][r*width + c];
  
            R = Co + Y + ((1-Cg)>>1) - (Co>>1);
            G = Y - ((-Cg)>>1);
            B = Y + ((1-Cg)>>1) - (Co>>1);

            R = CLIP(R, 0, data->ranges.max[0]);
            G = CLIP(G, 0, data->ranges.max[1]);
            B = CLIP(B, 0, data->ranges.max[2]);

            pixel_data->data[0][r*width + c] = R;
            pixel_data->data[1][r*width + c] = G;
            pixel_data->data[2][r*width + c] = B;
        }
    }
    return 0;
}

uint8_t ff_flif16_transform_permuteplanes_read(FLIF16TransformContext * ctx,
                                               FLIF16DecoderContext *s)
{
    transform_priv_permuteplanes* data = ctx->priv_data;
    data->ranges.num_planes = s->channels;

    switch (ctx->segment) {
        case 0:
            RAC_GET(s->rc, NULL, data->ctx_a, 1, &data->subtract, FLIF16_RAC_NZ_INT);
            //data->subtract = read_nz_int(rac, 0, 1);
            ++ctx->segment; __PLN__
            
            int p;
            for(p=0; p<4; p++){
                data->from[p] = 0;
                data->to[p] = 0;
            }
        case 1:
            for (; ctx->i < s->channels; ++ctx->i) {
                RAC_GET(s->rc, NULL, data->ctx_a, s->channels-1,
                        &data->permutation[ctx->i], 
                        FLIF16_RAC_NZ_INT);
                //data->permutation[p] = read_nz_int(s->rc, 0, s->channels-1);
                data->from[ctx->i] = 1;
                data->to[ctx->i] = 1;
            }
            ctx->i = 0;

            for (p = 0; p < s->channels; p++) {
                if(!data->from[p] || !data->to[p])
                return 0;
            }
            ++ctx->segment; __PLN__
            goto end;
    }

    end:
        return 1;

    need_more_data:
        return AVERROR(EAGAIN);
}

uint8_t ff_flif16_transform_permuteplanes_init(FLIF16TransformContext *ctx, 
                                               FLIF16DecoderContext *dec_ctx)
{
    transform_priv_permuteplanes *data = ctx->priv_data;
    
    data->ctx_a = ff_flif16_chancecontext_init();
    
    if(dec_ctx->channels< 3)
        return 0;
    if(  dec_ctx->src_ranges.min[0] < 1  
       ||dec_ctx->src_ranges.min[1] < 1
       ||dec_ctx->src_ranges.min[2] < 1) 
        return 0;

    int p;
    for (p = 0; p < data->ranges.num_planes; p++) {
        data->ranges.max[p] = dec_ctx->src_ranges.max[p];
        data->ranges.min[p] = dec_ctx->src_ranges.min[p];
    }
    return 1;
}

uint8_t ff_flif16_transform_permuteplanes_forward(
                                             FLIF16TransformContext *ctx,
                                             FLIF16DecoderContext *dec_ctx,
                                             FLIF16InterimPixelData * pixel_data)
{
    transform_priv_permuteplanes *data = ctx->priv_data;
    FLIF16ColorVal pixel[5];
    int r, c, p;
    int width  = dec_ctx->width;
    int height = dec_ctx->height;
    
    // Transforming pixel data.
    for (r=0; r<height; r++) {
        for (c=0; c<width; c++) {
            for (p=0; p<data->ranges.num_planes; p++)
                pixel[p] = pixel_data->data[p][r*width + c];
            pixel_data->data[0][r*width + c] = pixel[data->permutation[0]];
            if (!data->subtract){
                for (p=1; p<data->ranges.num_planes; p++)
                    pixel_data->data[p][r*width + c] = pixel[data->permutation[p]];
            }
            else{ 
                for(p=1; p<3 && p<data->ranges.num_planes; p++)
                    pixel_data->data[p][r*width + c] = pixel[data->permutation[p]] 
                                                    - pixel[data->permutation[0]];
                for(p=3; p<data->ranges.num_planes; p++)
                    pixel_data->data[p][r*width + c] = pixel[data->permutation[p]];
            }
        }
    }
    // Modifying ranges stored in transform context here.
    data->ranges.num_planes = dec_ctx->channels;
    if (data->subtract) {
        if (data->ranges.num_planes > 3) {
            data->ranges.min[3] = data->ranges.min[data->permutation[3]];
            data->ranges.max[3] = data->ranges.max[data->permutation[3]];
        }
        data->ranges.max[1] = data->ranges.max[1] - data->ranges.min[0];
        data->ranges.min[1] = data->ranges.min[1] - data->ranges.max[0];
        data->ranges.max[2] = data->ranges.max[2] - data->ranges.min[0];
        data->ranges.min[2] = data->ranges.min[2] - data->ranges.max[0];
    }
    return 1;
}

uint8_t ff_flif16_transform_permuteplanes_reverse(
                                        FLIF16TransformContext *ctx,
                                        FLIF16DecoderContext *dec_ctx,
                                        FLIF16InterimPixelData * pixels,
                                        uint32_t stride_row,
                                        uint32_t stride_col)
{   
    transform_priv_permuteplanes *data = ctx->priv_data;
    FLIF16ColorVal pixel[5];
    int height = dec_ctx->height;
    int width  = dec_ctx->width;
    int p, r, c;
    for (r=0; r<height; r+=stride_row) {
        for (c=0; c<width; c+=stride_col) {
            for (p=0; p<data->ranges.num_planes; p++)
                pixel[p] = pixels->data[p][r*width + c];
            for (p=0; p<data->ranges.num_planes; p++)
                pixels->data[data->permutation[p]][r*width + c] = pixel[p];
            
            pixels->data[data->permutation[0]][r*width + c] = pixel[0];
            if (!data->subtract) {
                for (p=1; p<data->ranges.num_planes; p++)
                    pixels->data[data->permutation[p]][r*width + c] = pixel[p];
            } else {
                for (p=1; p<3 && p<data->ranges.num_planes; p++)
                    pixels->data[data->permutation[p]][r*width + c] =
                    CLIP(pixel[p] + pixel[0],
                         data->ranges.min[data->permutation[p]],
                         data->ranges.max[data->permutation[p]]);
                for (p=3; p<data->ranges.num_planes; p++)
                    pixels->data[data->permutation[p]][r*width + c] = pixel[p];
            }
        }
    }
    return 1;
}

uint8_t ff_flif16_transform_channelcompact_read(FLIF16TransformContext * ctx,
                                                FLIF16DecoderContext *s)
{   
    transform_priv_channelcompact *data = ctx->priv_data;

    FLIF16ColorRanges srcRanges = s->src_ranges;

    unsigned int nb;
    int remaining;

    start:
    switch (ctx->segment) {
        case 0:
            if(!ctx->i){
                int p;
                for(p=0; p<4; p++){
                    data->CPalette[p]       = 0;
                    data->CPalette_size[p]  = 0;
                }
            }
            if(ctx->i < s->channels) {
                RAC_GET(s->rc, data->ctx_a,
                        0, srcRanges.max[ctx->i] - srcRanges.min[ctx->i],
                        &nb, FLIF16_RAC_NZ_INT);
                nb += 1;
                data->min = srcRanges.min[ctx->i];
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
                RAC_GET(s->rc, data->ctx_a,
                        0, srcRanges.max[ctx->i]-data->min-remaining,
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
        return 1;

    need_more_data:
        return AVERROR(EAGAIN);
}

uint8_t ff_flif16_transform_channelcompact_init(FLIF16TransformContext *ctx, 
                                                FLIF16DecoderContext *dec_ctx)
{
    if(dec_ctx->channels > 4)
        return 0;
    transform_priv_bounds *data = ctx->priv_data;
    data->ctx_a = ff_flif16_chancecontext_init();
    
    return 1;
}

uint8_t ff_flif16_transform_channelcompact_reverse(
                                        FLIF16TransformContext *ctx,
                                        FLIF16DecoderContext *s,
                                        FLIF16InterimPixelData * pixels,
                                        uint32_t stride_row,
                                        uint32_t stride_col)
{   
    transform_priv_channelcompact *data = ctx->priv_data;

    int p;
    uint32_t r, c;
    for(p=0; p<s->channels; p++){
        FLIF16ColorVal* palette = data->CPalette[p];
        unsigned int palette_size = data->CPalette_size[p];

        for(r=0; r < s->height; r++){
            for(c=0; c < s->width; c++){
                int P = pixels->data[p][r*s->width + c];
                if (P < 0 || P >= (int) palette_size)
                    P = 0;
                assert(P < (int) palette_size);
                pixels->data[p][r*s->width + c] = palette[P];
            }
        }
    }
    return 1;
}

uint8_t ff_flif16_transform_bounds_init(FLIF16TransformContext *ctx, 
                                       FLIF16DecoderContext *dec_ctx)
{
    if(dec_ctx->channels > 4)
        return 0;
    
    transform_priv_bounds *data = ctx->priv_data;
    data->ctx_a = ff_flif16_chancecontext_init();
    
    data->bounds[0] = av_mallocz(dec_ctx->channels*sizeof(FLIF16ColorVal));
    data->bounds[1] = av_mallocz(dec_ctx->channels*sizeof(FLIF16ColorVal));
    return 1;
}

uint8_t ff_flif16_transform_bounds_read(FLIF16TransformContext * ctx,
                                        FLIF16DecoderContext *s)
{
    FLIF16ColorRanges srcRanges = s->src_ranges;
    transform_priv_bounds *data = ctx->priv_data;
    FLIF16ColorVal max;

    start:
    if(ctx->i < s->channels){
        switch(ctx->segment){
            case 0:
                RAC_GET(s->rc, data->ctx_a,
                        srcRanges.min[ctx->i], srcRanges.max[ctx->i],
                        &data->min, FLIF16_RAC_GNZ_INT);
                ctx->segment++;
        
            case 1:
                RAC_GET(s->rc, data->ctx_a,
                        data->min, srcRanges.max[ctx->i],
                        &max, FLIF16_RAC_GNZ_INT);
                if(data->min > max)
                    return 0;
                if(data->min < srcRanges.min[ctx->i])
                    return 0;
                if(max > srcRanges.max[ctx->i])
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

FLIF16Transform flif16_transform_channelcompact = {
    .priv_data_size = sizeof(transform_priv_channelcompact),
    .init           = &ff_flif16_transform_channelcompact_init,
    .read           = &ff_flif16_transform_channelcompact_read,
    //.forward        = &ff_flif16_transform_channelcompact_forward,
    .reverse        = &ff_flif16_transform_channelcompact_reverse 
};

FLIF16Transform flif16_transform_ycocg = {
    .priv_data_size = sizeof(transform_priv_ycocg),
    .init           = &ff_flif16_transform_ycocg_init,
    .read           = NULL,
    .forward        = &ff_flif16_transform_ycocg_forward,
    .reverse        = &ff_flif16_transform_ycocg_reverse 
};

FLIF16Transform flif16_transform_permuteplanes = {
    .priv_data_size = sizeof(transform_priv_permuteplanes),
    .init           = &ff_flif16_transform_permuteplanes_init,
    .read           = &ff_flif16_transform_permuteplanes_read,
    .forward        = &ff_flif16_transform_permuteplanes_forward,
    .reverse        = &ff_flif16_transform_permuteplanes_reverse 
};

FLIF16Transform flif16_transform_bounds = {
    .priv_data_size = sizeof(transform_priv_bounds),
    .init           = &ff_flif16_transform_bounds_init,
    .read           = &ff_flif16_transform_bounds_read
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
                                                 FLIF16DecoderContext *s)
{
    FLIF16Transform *t = flif16_transforms[t_no];
    FLIF16TransformContext *c = av_mallocz(sizeof(FLIF16TransformContext));
    void *k = NULL;
    if(!c)
        return NULL;
    if (t->priv_data_size)
        k = av_mallocz(t->priv_data_size);
    c->priv_data = k;
    c->segment = 0;
    c->i = 0;

    if (t->init) {
        if(!t->init(c, s))
            return NULL;
    }
    
    return c;
}


int ff_flif16_transform_read(FLIF16TransformContext *c, FLIF16DecoderContext *s)
{
    if(!c)
        return 0;
    FLIF16Transform *t = flif16_transforms[c->t_no];
    if(t->read)
        return t->read(c, s);
    else
        return 1;
}
