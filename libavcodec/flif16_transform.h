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
 
#include <stdint.h>

#include "avcodec.h"
#include "flif16.h"
#include "libavutil/common.h"

#define MAX_PLANES 5

typedef int16_t FLIF16ColorVal;

typedef struct {
       FLIF16ColorVal min[MAX_PLANES], max[MAX_PLANES];
    int num_planes;
} FLIF16ColorRanges;

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
    FLIF16_TRANSFORM_FRAMELOOKBACK,
};

typedef struct FLIF16TransformContext{
    size_t priv_data_size;
    uint8_t done;
    FLIF16DecoderContext *dec_ctx;
    void *priv_data;
}FLIF16TransformContext;

typedef struct FLIF16Transform {
    uint8_t t_no;

    //Functions
    uint8_t (*init) (FLIF16TransformContext*, FLIF16ColorRanges*);
    uint8_t (*read) (FLIF16TransformContext*);
    uint8_t (*forward) (FLIF16TransformContext*, FLIF16InterimPixelData*);
    uint8_t (*reverse) (FLIF16TransformContext*, FLIF16InterimPixelData*);

    FLIF16TransformContext *transform_ctx;
} FLIF16Transform;

FLIF16Transform flif16_transform_ycocg = {
    .t_no    = FLIF16_TRANSFORM_YCOCG,
    .init    = ff_flif16_transform_ycocg_init,
    .read    = ff_flif16_transform_ycocg_read,
    .forward = ff_flif16_transform_ycocg_forward,
    .reverse = ff_flif16_transform_ycocg_reverse 
};

FLIF16Transform flif16_transform_permuteplanes = {
    .t_no    = FLIF16_TRANSFORM_PERMUTEPLANES,
    .init    = ff_flif16_transform_permuteplanes_init,
    .read    = ff_flif16_transform_permuteplanes_read,
    .forward = ff_flif16_transform_permuteplanes_forward,
    //.reverse = ff_flif16_transform_permuteplanes_reverse 
};

FLIF16Transform *flif16_transforms[] = {
    &flif16_transform_ycocg,
    &flif16_transform_permuteplanes,
};

typedef struct{
    uint8_t initialized;            //FLAG : initialized or not.
    int height, width;
    FLIF16ColorVal *data[MAX_PLANES];
    FLIF16ColorRanges ranges;
} FLIF16InterimPixelData;

FLIF16ColorRanges* ff_get_ranges_ycocg( FLIF16InterimPixelData *pixelData,
                                        FLIF16ColorRanges *ranges){
    int p = pixelData->ranges.num_planes;
    ranges->num_planes = p;
    int i, c, r, width, height;
    FLIF16ColorVal min, max;
    width = pixelData->width;
    height = pixelData->height;
    for(i=0; i<p; i++){
        min = pixelData->data[p][0];
        max = pixelData->data[p][0];
        for(r=0; r<pixelData->height; r++){
            for(c=0; c<pixelData->width; c++){
                if(min > pixelData->data[p][r*width + c])
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
static inline int ff_get_min_y(int origmax4){
    return 0;
}

static inline int ff_get_max_y(int origmax4){
    return 4*origmax4-1;
}

static inline int ff_get_min_co(int origmax4, int yval){
    int newmax = 4*origmax4 - 1;
    if (yval < origmax4-1)
        return -3 - 4*yval; 
    else if (yval >= 3*origmax4)
        return 4*(yval - newmax);
    else
        return -newmax;
}

static inline int ff_get_max_co(int origmax4, int yval){
    int newmax = 4*origmax4 - 1;
    if (yval < origmax4-1)
        return 3 + 4*yval; 
    else if (yval >= 3*origmax4)
        return 4*(newmax - yval);
    else
        return newmax;
}

static inline int ff_get_min_cg(int origmax4, int yval, int coval){
    int newmax = 4*origmax4 - 1;
    if (yval < origmax4-1)
        return -2 - 2*yval; 
    else if (yval >= 3*origmax4)
        return -2*(newmax-yval) + 2*((abs(coval)+1)/2);
    else{
        return min(2*yval + 1, 2*newmax - 2*yval - 2*abs(coval)+1)/2;
    }
}

static inline int ff_get_max_cg(int origmax4, int yval, int coval){
    int newmax = 4*origmax4 - 1;
    if (yval < origmax4-1)
        return 1 + 2*yval - 2*(abs(coval)/2); 
    else if (yval >= 3*origmax4)
        return 2 * (newmax-yval);
    else
        return min(2*(yval- newmax), -2*yval - 1 + 2*(abs(coval)/2));
}

static inline int ff_min_range_ycocg(int p, int origmax4){
    switch(p){
        case 0:
            return 0;
        case 1:
            return -4*origmax4+1;
        case 2:
            return -4*origmax4+1;
        default:
            return 0;
            break;
    }
}

static inline int ff_max_range_ycocg(int p, int origmax4){
    switch(p){
        case 0:
            return 4*origmax4-1;
        case 1:
            return 4*origmax4-1;
        case 2:
            return 4*origmax4-1;
        default:
            return 0;
            break;
    }
}


FLIF16Transform* ff_flif16_transform_process(int t_no, FLIF16DecoderContext *s);
