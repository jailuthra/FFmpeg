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
#define CLIP(x,l,u) ((x) > (u) ? (u) : ((x) < (l) ? (l) : (x))
 
uint8_t ff_flif16_transform_ycocg_read(FLIF16TransformContext *ctx)
{
    //It needs to store int origmax4 and FLIF16ColorRanges ranegs.
    ctx->priv_data_size = sizeof(int) + sizeof(FLIF16ColorRanges);
    ctx->priv_data = av_mallocz(ctx->priv_data_size);
    FLIF16ColorRanges *ranges = (FLIF16ColorRanges *)(ctx->priv_data
                                                    + sizeof(int));
    ranges->num_planes = ctx->dec_ctx->channels;
    return 1;
}

uint8_t ff_flif16_transform_ycocg_init(FLIF16TransformContext *ctx, 
                                       FLIF16ColorRanges *srcRanges)
{
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

    int *origmax4 = (int *) ctx->priv_data;
    *origmax4 = FFMAX3(srcRanges->max[0], 
                        srcRanges->max[1], 
                        srcRanges->max[2])/4 -1;

    FLIF16ColorRanges *ranges = (FLIF16ColorRanges *) (ctx->priv_data
                                                     + sizeof(int));
    int p;
    for (p = 0; p < ranges->num_planes; p++) {
        ranges->max[p] = srcRanges->max[p];
        ranges->min[p] = srcRanges->min[p];
    }
    return 1;
}

uint8_t ff_flif16_transform_ycocg_forward(FLIF16TransformContext *ctx,
                                          FLIF16InterimPixelData * pixelData)
{
    int r, c;
    FLIF16ColorVal R,G,B,Y,Co,Cg;

    int height = pixelData->height;
    int width = pixelData->width;

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

    int *origmax4 = (int *)ctx->priv_data;
    FLIF16ColorRanges *ranges = (FLIF16ColorRanges *)(ctx->priv_data
                                                    + sizeof(int));
    // Will see if ranges need to be stored separately in transform struct only.
    int p;
    for (p = 0; p < ranges->num_planes; p++) {
        ranges->max[p] = ff_max_range_ycocg(p, *origmax4);
        ranges->min[p] = ff_min_range_ycocg(p, *origmax4);
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
    int r, c;
    FLIF16ColorVal R,G,B,Y,Co,Cg;
    FLIF16ColorRanges *ranges = (FLIF16ColorRanges *)(ctx->priv_data
                                                    + sizeof(int));
    int height = pixelData->height;
    int width = pixelData->width;

    for (r=0; r<height; r+=strideRow) {
        for (c=0; c<width; c+=strideCol) {
            Y  = pixelData->data[0][r*width + c];
            Co = pixelData->data[1][r*width + c];
            Cg = pixelData->data[2][r*width + c];
  
            R = Co + Y + ((1-Cg)>>1) - (Co>>1);
            G = Y - ((-Cg)>>1);
            B = Y + ((1-Cg)>>1) - (Co>>1);

            CLIP(R, 0, ranges->max[0]);
            CLIP(G, 0, ranges->max[1]);
            CLIP(B, 0, ranges->max[2]);

            pixelData->data[0][r*width + c] = R;
            pixelData->data[1][r*width + c] = G;
            pixelData->data[2][r*width + c] = B;
        }
    }
    return 0;
}

uint8_t ff_flif16_transform_permuteplanes_read(FLIF16TransformContext * ctx)
{
    //It needs to store uint8_t subtract flag, uint8_t permutation[5] and
    //FLIF16ColorRanges ranges. 
    ctx->priv_data_size = 6 * sizeof(uint8_t) + sizeof(FLIF16ColorRanges);
    ctx->priv_data = av_mallocz(ctx->priv_data_size);
    uint8_t *subtract = (uint8_t *) ctx->priv_data;
    uint8_t *permutation = (uint8_t *) (ctx->priv_data + sizeof(uint8_t));
    FLIF16ColorRanges *ranges = (FLIF16ColorRanges *) (ctx->priv_data +
                                                       6 * sizeof(uint8_t));
    ranges->num_planes = ctx->dec_ctx->channels;
    FLIF16RangeCoder* rac = ctx->dec_ctx->rc;
    *subtract = ff_flif16_rac_read_nz_int(rac, 0, 1);
    uint8_t from[4] = {0, 0, 0, 0}, to[4] = {0, 0, 0, 0};
    int p;
    int planes = ranges->num_planes;
    for (p = 0; p < planes; p++) {
        permutation[p] = ff_flif16_rac_read_nz_int(rac, 0, planes-1);
        from[p] = 1;
        to[p] = 1;
    }
    for (p = 0; p < planes; p++) {
        if(!from[p] || !to[p])
            return 0;
    }
    return 1;
}

uint8_t ff_flif16_transform_permuteplanes_init(FLIF16TransformContext *ctx, 
                                               FLIF16ColorRanges *srcRanges){
    if(srcRanges->num_planes < 3)
        return 0;
    if(srcRanges->min[0] < 1  || srcRanges->min[1] < 1 || srcRanges->min[2] < 1) 
        return 0;
    FLIF16ColorRanges *ranges = (FLIF16ColorRanges *) (ctx->priv_data 
                                                     + sizeof(int));
    int p;
<<<<<<< HEAD
    for(p = 0; p < ranges->num_planes; p++){
=======
    for (p = 0; p < ranges->num_planes; p++) {
>>>>>>> 9194dba269ca249010b09aec3d67a8f33e52c989
        ranges->max[p] = srcRanges->max[p];
        ranges->min[p] = srcRanges->min[p];
    }
    return 1;
}

uint8_t ff_flif16_transform_permuteplanes_forward(
                                             FLIF16TransformContext *ctx,
                                             FLIF16InterimPixelData * pixelData)
{

    uint8_t *subtract = (uint8_t *)ctx->priv_data;
    uint8_t *permutation = (uint8_t *)(ctx->priv_data + sizeof(uint8_t));
    FLIF16ColorRanges *ranges = (FLIF16ColorRanges *)(ctx->priv_data 
                                                    + 6*sizeof(uint8_t));
    FLIF16ColorVal pixel[5];
    int r, c, p;
    int width = pixelData->width;
    int height = pixelData->height;
    
    // Transforming pixel data.
    for (r=0; r<height; r++) {
        for (c=0; c<width; c++) {
            for (p=0; p<ranges->num_planes; p++)
                pixel[p] = pixelData->data[p][r*width + c];
            pixelData->data[0][r*width + c] = pixel[permutation[0]];
            if (!(*subtract)){
                for (p=1; p<ranges->num_planes; p++)
                    pixelData->data[p][r*width + c] = pixel[permutation[p]];
            }
            else{ 
                for(p=1; p<3 && p<ranges->num_planes; p++)
                    pixelData->data[p][r*width + c] = pixel[permutation[p]] 
                                                    - pixel[permutation[0]];
                for(p=3; p<ranges->num_planes; p++)
                    pixelData->data[p][r*width + c] = pixel[permutation[p]];
            }
        }
    }
    // Modifying ranges stored in transform context here.
    ranges->num_planes = ctx->dec_ctx->channels;
    if (*subtract) {
        if (ranges->num_planes > 3) {
            ranges->min[3] = ranges->min[permutation[3]];
            ranges->max[3] = ranges->max[permutation[3]];
        }
        ranges->max[1] = ranges->max[1] - ranges->min[0];
        ranges->min[1] = ranges->min[1] - ranges->max[0];
        ranges->max[2] = ranges->max[2] - ranges->min[0];
        ranges->min[2] = ranges->min[2] - ranges->max[0];
    }
    return 1;
}

uint8_t ff_flif16_transform_permuteplanes_reverse(
                                        FLIF16TransformContext *ctx,
                                        FLIF16InterimPixelData * pixelData,
                                        uint32_t strideRow,
                                        uint32_t strideCol){
    uint8_t *subtract = (uint8_t *)ctx->priv_data;
    uint8_t *permutation = (uint8_t *)(ctx->priv_data + sizeof(uint8_t));
    FLIF16ColorRanges *ranges = (FLIF16ColorRanges *)(ctx->priv_data 
                                                    + 6*sizeof(uint8_t));
    FLIF16ColorVal pixel[5];
    int height = pixelData->height;
    int width = pixelData->width;
    int p, r, c;
    for (r=0; r<height; r+=strideRow) {
        for (c=0; c<width; c+=strideCol) {
            for (p=0; p<ranges->num_planes; p++)
                pixel[p] = pixelData->data[p][r*width + c];
            for (p=0; p<ranges->num_planes; p++)
                pixelData->data[permutation[p]][r*width + c] = pixel[p];
            
            pixelData->data[permutation[0]][r*width + c] = pixel[0];
            if (!(*subtract)) {
                for (p=1; p<ranges->num_planes; p++)
                    pixelData->data[permutation[p]][r*width + c] = pixel[p];
<<<<<<< HEAD
            }
            else{
                for(p=1; p<3 && p<ranges->num_planes; p++)
                    pixelData->data[permutation[p]][r*width + c] = 
                    clip(pixel[p] + pixel[0],
                         ranges->min[permutation[p]],
                         ranges->max[permutation[p]]);
                for(p=3; p<ranges->num_planes; p++)
=======
            } else {
                for (p=1; p<3 && p<ranges->num_planes; p++)
                    pixelData->data[permutation[p]][r*width + c] =         \
                    CLIP(pixel[p] + pixel[0], ranges->min[permutation[p]], \
                         ranges->max[permutation[p]]);
                for (p=3; p<ranges->num_planes; p++)
>>>>>>> 9194dba269ca249010b09aec3d67a8f33e52c989
                    pixelData->data[permutation[p]][r*width + c] = pixel[p];
            }
        }
    }
}

// t_no is likely useless
FLIF16Transform flif16_transform_ycocg = {
    .t_no    = FLIF16_TRANSFORM_YCOCG,
    .init    = &ff_flif16_transform_ycocg_init,
    .read    = &ff_flif16_transform_ycocg_read,
    .forward = &ff_flif16_transform_ycocg_forward,
    .reverse = &ff_flif16_transform_ycocg_reverse 
};

FLIF16Transform flif16_transform_permuteplanes = {
    .t_no    = FLIF16_TRANSFORM_PERMUTEPLANES,
    .init    = &ff_flif16_transform_permuteplanes_init,
    .read    = &ff_flif16_transform_permuteplanes_read,
    .forward = &ff_flif16_transform_permuteplanes_forward,
    .reverse = &ff_flif16_transform_permuteplanes_reverse 
};


// The indices in the array map to the respective transform number.
FLIF16Transform *flif16_transforms[13] = {
    NULL, // FLIF16_TRANSFORM_CHANNELCOMPACT = 0,
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

FLIF16Transform* process(int t_no, FLIF16DecoderContext *s)
{
    FLIF16Transform *t = flif16_transforms[t_no];
    // Unlike C++, the void pointer returned by malloc is automatically
    // converted to the desired type of LHS (since all pointer types are
    // ultimately void). C++ however requires you to cast your pointers.
    t->transform_ctx = (FLIF16TransformContext*)
                       av_mallocz(sizeof(FLIF16TransformContext));
    t->transform_ctx->dec_ctx = s;
    t->read(t->transform_ctx);
    return t;
}
