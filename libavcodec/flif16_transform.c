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

// Replace by av_clip functions
#define CLIP(x,l,u) (x) > (u) ? (u) : ((x) < (l) ? (l) : (x))

FLIF16ColorRanges* ff_get_ranges( FLIF16InterimPixelData *pixelData,
                                        FLIF16ColorRanges *ranges){
    int p = pixelData->ranges.num_planes;
    ranges->num_planes = p;
    int i, c, r, width, height;
    FLIF16ColorVal min, max;
    width = pixelData->width;
    height = pixelData->height;
    for (i=0; i<p; i++) {
        min = pixelData->data[p][0];
        max = pixelData->data[p][0];
        for (r=0; r<pixelData->height; r++) {
            for(c=0; c<pixelData->width; c++) {
                if (min > pixelData->data[p][r*width + c])
                    min = pixelData->data[p][r*width + c];
                if(max < pixelData->data[p][r*width + c])
                    max = pixelData->data[p][r*width + c];
            }
        }
        ranges->min[p] = min;
        ranges->max[p] = max;
    }
    return ranges;
}
 
uint8_t ff_flif16_transform_ycocg_read(FLIF16TransformContext *ctx)
{
    ff_transform_priv_ycocg *data = ctx->priv_data;
    data->ranges.num_planes = ctx->dec_ctx->channels;
    return 1;
}

uint8_t ff_flif16_transform_ycocg_init(FLIF16TransformContext *ctx, 
                                       FLIF16ColorRanges *srcRanges)
{   
    ff_transform_priv_ycocg *data = ctx->priv_data;
    if(srcRanges->num_planes < 3) 
        return 0;
    
    if(   srcRanges->min[0] == srcRanges->max[0] 
       || srcRanges->min[1] == srcRanges->max[1] 
       || srcRanges->min[2] == srcRanges->max[2])
        return 0;
    
    if(   srcRanges->min[0] < 1 
       || srcRanges->min[1] < 1 
       || srcRanges->min[2] < 1) 
        return 0;

    data->origmax4 = FFMAX3(srcRanges->max[0], 
                        srcRanges->max[1], 
                        srcRanges->max[2])/4 -1;

    int p;
    for (p = 0; p < data->ranges.num_planes; p++) {
        data->ranges.max[p] = srcRanges->max[p];
        data->ranges.min[p] = srcRanges->min[p];
    }
    return 1;
}

uint8_t ff_flif16_transform_ycocg_forward(FLIF16TransformContext *ctx,
                                          FLIF16InterimPixelData * pixelData)
{
    ff_transform_priv_ycocg *data = ctx->priv_data;
    int r, c;
    FLIF16ColorVal R,G,B,Y,Co,Cg;

    int height = ctx->dec_ctx->height;
    int width = ctx->dec_ctx->width;

    for (r=0; r<height; r++) {
        for (c=0; c<width; c++) {
            R = pixelData->data[0][r*width + c];
            G = pixelData->data[1][r*width + c];
            B = pixelData->data[2][r*width + c];

            Y = (((R + B)>>1) + G)>>1;
            Co = R - B;
            Cg = G - ((R + B)>>1);

            pixelData->data[0][r*width + c] = Y;
            pixelData->data[1][r*width + c] = Co;
            pixelData->data[2][r*width + c] = Cg;
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
                                          FLIF16InterimPixelData * pixelData,
                                          uint32_t strideRow,
                                          uint32_t strideCol)
{
    ff_transform_priv_ycocg *data = ctx->priv_data;
    int r, c;
    FLIF16ColorVal R,G,B,Y,Co,Cg;
    int height = ctx->dec_ctx->height;
    int width = ctx->dec_ctx->width;

    for (r=0; r<height; r+=strideRow) {
        for (c=0; c<width; c+=strideCol) {
            Y  = pixelData->data[0][r*width + c];
            Co = pixelData->data[1][r*width + c];
            Cg = pixelData->data[2][r*width + c];
  
            R = Co + Y + ((1-Cg)>>1) - (Co>>1);
            G = Y - ((-Cg)>>1);
            B = Y + ((1-Cg)>>1) - (Co>>1);

            CLIP(R, 0, data->ranges.max[0]);
            CLIP(G, 0, data->ranges.max[1]);
            CLIP(B, 0, data->ranges.max[2]);

            pixelData->data[0][r*width + c] = R;
            pixelData->data[1][r*width + c] = G;
            pixelData->data[2][r*width + c] = B;
        }
    }
    return 0;
}

uint8_t ff_flif16_transform_permuteplanes_read(FLIF16TransformContext * ctx)
{
    ff_transform_priv_permuteplanes* data = ctx->priv_data;
    data->ranges.num_planes = ctx->dec_ctx->channels;
    FLIF16DecoderContext* s = ctx->dec_ctx;

    switch (s->segment) {
        case 0:
            RAC_GET(s->rc, NULL, 0, 1, &data->subtract, FLIF16_RAC_NZ_INT);
            //data->subtract = read_nz_int(rac, 0, 1);
            ++s->segment; __PLN__
        
        case 1:
            uint8_t from[4] = {0, 0, 0, 0}, to[4] = {0, 0, 0, 0};
            int p;
            for (p = 0; p < s->channels; p++, ++s->i) {
                RAC_GET(s->rc, NULL, 0, s->channels-1, &data->permutation[p], 
                        FLIF16_RAC_NZ_INT);
                //data->permutation[p] = read_nz_int(s->rc, 0, s->channels-1);
                from[p] = 1;
                to[p] = 1;
            }
            s->i = 0;
            for (p = 0; p < s->channels; p++) {
                if(!from[p] || !to[p])
                return 0;
            }
            ++s->segment; __PLN__
            goto end;
    }

    end:
        return 1;

    need_more_data:
        return AVERROR(EAGAIN);
}

uint8_t ff_flif16_transform_permuteplanes_init(FLIF16TransformContext *ctx, 
                                               FLIF16ColorRanges *srcRanges)
{
    ff_transform_priv_ycocg *data = ctx->priv_data;
    if(srcRanges->num_planes < 3)
        return 0;
    if(srcRanges->min[0] < 1  || srcRanges->min[1] < 1 || srcRanges->min[2] < 1) 
        return 0;

    int p;
    for (p = 0; p < data->ranges.num_planes; p++) {
        data->ranges.max[p] = srcRanges->max[p];
        data->ranges.min[p] = srcRanges->min[p];
    }
    return 1;
}

uint8_t ff_flif16_transform_permuteplanes_forward(
                                             FLIF16TransformContext *ctx,
                                             FLIF16InterimPixelData * pixelData)
{
    ff_transform_priv_permuteplanes *data = ctx->priv_data;
    FLIF16ColorVal pixel[5];
    int r, c, p;
    int width = ctx->dec_ctx->width;
    int height = ctx->dec_ctx->height;
    
    // Transforming pixel data.
    for (r=0; r<height; r++) {
        for (c=0; c<width; c++) {
            for (p=0; p<data->ranges.num_planes; p++)
                pixel[p] = pixelData->data[p][r*width + c];
            pixelData->data[0][r*width + c] = pixel[data->permutation[0]];
            if (!data->subtract){
                for (p=1; p<data->ranges.num_planes; p++)
                    pixelData->data[p][r*width + c] = pixel[data->permutation[p]];
            }
            else{ 
                for(p=1; p<3 && p<data->ranges.num_planes; p++)
                    pixelData->data[p][r*width + c] = pixel[data->permutation[p]] 
                                                    - pixel[data->permutation[0]];
                for(p=3; p<data->ranges.num_planes; p++)
                    pixelData->data[p][r*width + c] = pixel[data->permutation[p]];
            }
        }
    }
    // Modifying ranges stored in transform context here.
    data->ranges.num_planes = ctx->dec_ctx->channels;
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
                                        FLIF16InterimPixelData * pixels,
                                        uint32_t strideRow,
                                        uint32_t strideCol)
{   
    ff_transform_priv_permuteplanes *data = ctx->priv_data;
    FLIF16ColorVal pixel[5];
    int height = ctx->dec_ctx->height;
    int width  = ctx->dec_ctx->width;
    int p, r, c;
    for (r=0; r<height; r+=strideRow) {
        for (c=0; c<width; c+=strideCol) {
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

uint8_t ff_flif16_transform_channelcompact_read(FLIF16TransformContext * ctx)
{   
    ff_transform_priv_channelcompact *data = ctx->priv_data;
    unsigned int i;
    for(i=0; i<4; i++){
        data->CPalette[i]        = 0;
        data->CPalette_size[i]   = 0;
    }    
    FLIF16ColorVal min;
    FLIF16DecoderContext *s = ctx->dec_ctx;
    unsigned int nb;
    switch (ctx->segment) {
        case 0:
            int p;
            for (p=0; p < s->channels; p++, ++s->i) {
                RAC_GET(s->rc, NULL, 0, 255, &nb, FLIF16_RAC_NZ_INT);
                nb += 1;
                data->CPalette[p] = av_mallocz(nb*sizeof(FLIF16ColorVal));
                data->CPalette_size[p] = nb;
                min = 0;
                int remaining = nb-1;
                for (unsigned int i=0; i<nb; i++) {
                    RAC_GET(s->rc, NULL, 0, 255-min-remaining,
                            &data->CPalette[p][i], 
                            FLIF16_RAC_NZ_INT);
                    data->CPalette[p][i] += min;
                    //Basically I want to perform this operation :
                    //CPalette[p][i] = min + read_nz_int(0, 255-min-remaining);
                    min = data->CPalette[p][i]+1;
                    remaining--;
                }
            }
            s->i = 0;    
            ++s->segment;
            goto end;
    }
    
    end:
        return 1;

    need_more_data:
        return AVERROR(EAGAIN);
}

uint8_t ff_flif16_transform_channelcompact_init(FLIF16TransformContext *ctx, 
                                                FLIF16ColorRanges *srcRanges)
{
    if(srcRanges->num_planes > 4)
        return 0;
    return 1;
}

uint8_t ff_flif16_transform_channelcompact_reverse(
                                        FLIF16TransformContext *ctx,
                                        FLIF16InterimPixelData * pixels,
                                        uint32_t strideRow,
                                        uint32_t strideCol)
{   
    ff_transform_priv_channelcompact *data = ctx->priv_data;
    FLIF16DecoderContext *s = ctx->dec_ctx;

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

FLIF16Transform flif16_transform_ycocg = {
    .priv_data_size = sizeof(ff_transform_priv_ycocg),
    .init           = &ff_flif16_transform_ycocg_init,
    .read           = &ff_flif16_transform_ycocg_read,
    .forward        = &ff_flif16_transform_ycocg_forward,
    .reverse        = &ff_flif16_transform_ycocg_reverse 
};

FLIF16Transform flif16_transform_permuteplanes = {
    .priv_data_size = sizeof(ff_transform_priv_permuteplanes),
    .init           = &ff_flif16_transform_permuteplanes_init,
    .read           = &ff_flif16_transform_permuteplanes_read,
    .forward        = &ff_flif16_transform_permuteplanes_forward,
    .reverse        = &ff_flif16_transform_permuteplanes_reverse 
};

FLIF16Transform flif16_transform_channelcompact = {
    .priv_data_size = sizeof(ff_transform_priv_channelcompact),
    .init           = &ff_flif16_transform_channelcompact_init,
    .read           = &ff_flif16_transform_channelcompact_read,
    //.forward        = &ff_flif16_transform_channelcompact_forward,
    .reverse        = &ff_flif16_transform_channelcompact_reverse 
};

FLIF16Transform *flif16_transforms[13] = {
    &flif16_transform_channelcompact,
    &flif16_transform_ycocg,
    NULL, // FLIF16_TRANSFORM_RESERVED1,
    &flif16_transform_permuteplanes,
    NULL, // FLIF16_TRANSFORM_BOUNDS,
    NULL, // FLIF16_TRANSFORM_PALETTEALPHA,
    NULL, // FLIF16_TRANSFORM_PALETTE,
    NULL, // FLIF16_TRANSFORM_COLORBUCKETS,
    NULL, // FLIF16_TRANSFORM_RESERVED2,
    NULL, // FLIF16_TRANSFORM_RESERVED3,
    NULL, // FLIF16_TRANSFORM_DUPLICATEFRAME,
    NULL, // FLIF16_TRANSFORM_FRAMESHAPE,
    NULL  // FLIF16_TRANSFORM_FRAMELOOKBACK
};

FLIF16TransformContext* ff_flif16_transform_process(int t_no,
                                                    FLIF16DecoderContext *s)
{
    FLIF16Transform *t = flif16_transforms[t_no];
    FLIF16TransformContext *c = av_mallocz(sizeof(FLIF16TransformContext));
    c->dec_ctx = s;
    c->priv_data = av_mallocz(sizeof(t->priv_data_size));
    t->read(c);
    return c;
}
