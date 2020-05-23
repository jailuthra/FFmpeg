/*
 * Transforms for FLIF16.
 * Copyright (c) 2020 Kartik K. Khullar <kartikkhullar@gmail.com>
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

#include "../libavutil/frame.h"
#include "flif16.h"

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
}

char* flif16_transform_desc[] = {
    "ChannelCompact",
    "YCoCg", 
    "?",
    "Permute",
    "Bounds",
    "PaletteAlpha",
    "Palette", 
    "ColorBuckets", 
    "?", 
    "?", 
    "DuplicateFrame", 
    "FrameShape", 
    "FrameLookback"
};

typedef struct FLIF16Transform {
    char* desc;                     //Description of FLIF16Transform
    uint8_t FLIF16Transform_number;       
    uint8_t done;
    FLIF16DecoderContext *s;
    int data_size;
    void *data;
} FLIF16Transform;

typedef struct{
    uint8_t initialized;            //FLAG : initialized or not.
    int height, width;
    FLIF16ColorVal *data[MAX_PLANES];
    FLIF16ColorRanges ranges;
} FLIF16InterimPixelData;

FLIF16ColorRanges* getRanges(FLIF16InterimPixelData* pixelData, 
                             FLIF16ColorRanges *ranges);

int getmax(FLIF16ColorRanges* ranges, int p);

// All of these are local functions, which are never used outsider the internal
// functions of the transforms. Please prefix the function names with ff_*
// and the return values with static. For example, max_range_YCoCg becomes:
//      static int ff_max_range_ycocg(int p, int origmax 4.
// You can also make these functions inline, if the function is small enough
// which will increase speed.
// All private functions have the ff_* prefix. Also the names must be kept in
// lowercase as per convention.

// Also, indentation is 4 spaces. Your editor is inserting tabs instead. Please
// configure your editor to do so. Tabs are not of uniform width across editors.

// Please try to keep everything within 80 columns by breaking long statements.
// Your editor may hae an option to draw a vertical line at 80 columns.

// This commit will most likely prevent this code from compiling. If you want to
// revert to the previous version, please use git reset.

// You might have missed my comment on gitbub over here:
// https://github.com/daujerrine/ffmpeg/commit/341ce0ef2e00ee3f5ae9cc8dfd3b9d21d1686476#commitcomment-39077413
// You might have to click on "watch" in order to get email notifications for
// comments.

// Please remove these comments afterwards.

int max_range_YCoCg(int p, int origmax4);
int min_range_YCoCg(int p, int origmax4);

int get_min_y(int);
int get_max_y(int orgimax4);

int get_min_co(int orgimax4, int yval);
int get_max_co(int orgimax4, int yval);

int get_min_cg(int orgimax4, int yval, int coval);
int get_max_cg(int orgimax4, int yval, int coval);

int min(int, int);  //* Replace by FF_MIN
int max(int, int, int); //* Replace by FF_MAX3 https://ffmpeg.org/doxygen/trunk/common_8h.html

//FLIF16ColorRanges crangesYCoCg(int p, FLIF16ColorVal* prevPlanes, FLIF16TransformYCoCg FLIF16Transform);

FLIF16Transform* ff_flif16_transform_process(int t_no, FLIF16DecoderContext *s);

uint8_t ff_flif16_transform_read(FLIF16Transform *transform);
uint8_t ff_flif16_transform_init(FLIF16Transform *transform,
                                 FLIF16ColorRanges *ranges);
uint8_t ff_flif16_transform_forward(FLIF16Transform *transform, 
                                    FLIF16InterimPixelData *pixelData);
uint8_t ff_flif16_transform_reverse(FLIF16Transform *transform, 
                                    FLIF16InterimPixelData *pixelData);


/*
Maybe write it like this:

typedef struct FLIF16TransformContext {
    uint8_t done;
    FLIF16DecoderContext *s;
    void *priv_data;
}

typedef struct FLIF16Transform {
    char desc[15];
    // These are functions
    uint8_t (*init) (FLIF16TransformContext *);
    uint8_t (*read) (FLIF16TransformContext *);
    uint8_t (*forward) (FLIF16TransformContext *);
    uint8_t (*reverse) (FLIF16TransformContext *);
    size_t priv_data_size;
} FLIF16Transform;

FLIF16Transform *flif16_transforms[] {
    ...
    &flif16_transform_ycocg,
    &flif16_transform_permuteplanes,
    ...
}

int ff_flif16_transform_process(unsigned int t_no, int direction, 
                                FLIF16DecoderContext *s)
{
    FLIF16Transform *t = flif16_transforms[t_no];
    FLIF16TransformContext ctx = {
        .priv_data = av_mallocz(t->priv_data_size);
        .s = s; // Maybe use a more elaborate name
    }

    // Perform error handling as required over here by returning an integer
    t->init(&ctx);
    t->read(&ctx);
    t->forward(&ctx);
    t->reverse(&ctx);
    
    return 1;
}
 
*/
