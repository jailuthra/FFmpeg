/*
 * FLIF16 Image Format Definitions
 * Copyright (c) 2020 Anamitra Ghorui <aghorui@teknik.io>
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
 * FLIF16 format definitions and functions.
 */

#ifndef AVCODEC_FLIF16_H
#define AVCODEC_FLIF16_H

#include <stdint.h>
#include <stdlib.h>

#include "avcodec.h"
#include "libavutil/pixfmt.h"

#include "flif16_rangecoder.h"

// Remove these
//#define __PLN__ printf("At: [%s] %s, %d\n", __func__, __FILE__, __LINE__);
//#define MSG(fmt, ...) printf("[%s] " fmt, __func__, ##__VA_ARGS__)
//#include <assert.h>
//#define __PLN__ #error remove me
//#define MSG(fmt,...) #error remove me

#define FF_FLIF16_VARINT_APPEND(a,x) a = (a << 7) | (uint64_t) (x & 127)

#define MAX_PLANES 5

static const uint8_t flif16_header[4] = "FLIF";

struct FLIF16DecoderContext;
typedef struct FLIF16DecoderContext FLIF16DecoderContext;

typedef int32_t FLIF16ColorVal;

struct FLIF16Ranges;
typedef struct FLIF16Ranges FLIF16Ranges;

typedef struct FLIF16RangesContext{
    uint8_t r_no;
    uint8_t num_planes;
    void* priv_data;
} FLIF16RangesContext;

typedef struct FLIF16Ranges {
    uint8_t priv_data_size;

    FLIF16ColorVal (*min)(FLIF16RangesContext*, int);
    FLIF16ColorVal (*max)(FLIF16RangesContext*, int);
    void (*minmax)(FLIF16RangesContext*, const int, FLIF16ColorVal*,
                   FLIF16ColorVal*, FLIF16ColorVal*);
    void (*snap)(FLIF16RangesContext*, const int, FLIF16ColorVal*,
                 FLIF16ColorVal*, FLIF16ColorVal*, FLIF16ColorVal*);
    uint8_t is_static;
    void (*close)(FLIF16RangesContext*);
    void (*previous)(FLIF16RangesContext*);
} FLIF16Ranges;


// Each FLIF16PixelData Struct will contain a single frame
// This will work similarly to AVFrame.
// **data will carry an array of planes
// Bounds of these planes will be defined by width and height
// If required, linesize[], similar to AVFrame can be defined.
// If Width, height, and number of planes of each frame is Constant, then
// having numplanes, width, height is redundant. Check.
typedef struct FLIF16PixelData {
    uint8_t initialized;            //FLAG : initialized or not. // See initialisation with NULL check instead
    uint8_t num_planes;
    uint32_t height, width;
    uint8_t *is_constant;
    void **data;
} FLIF16PixelData;

typedef struct FLIF16TransformContext{
    uint8_t t_no;
    unsigned int segment;     //segment the code is executing in.
    int i;                    //variable to store iteration number.
    uint8_t done;
    void *priv_data;
} FLIF16TransformContext;

typedef struct FLIF16Transform {
    uint8_t priv_data_size;
    //Functions
    uint8_t (*init) (FLIF16TransformContext*, FLIF16RangesContext*);
    uint8_t (*read) (FLIF16TransformContext*, FLIF16DecoderContext*, FLIF16RangesContext*);
    FLIF16RangesContext* (*meta) (FLIF16TransformContext*, FLIF16RangesContext*);
    uint8_t (*forward) (FLIF16TransformContext*, FLIF16PixelData*);
    uint8_t (*reverse) (FLIF16TransformContext*, FLIF16PixelData*, uint32_t, uint32_t);
    void (*close) (FLIF16TransformContext*);
} FLIF16Transform;

typedef struct FLIF16DecoderContext {
    GetByteContext gb;
    FLIF16MANIACContext maniac_ctx;
    FLIF16RangeCoder rc;

    // For now, we will use this to store output
    FLIF16PixelData  *out_frames;
    
    uint8_t buf[FLIF16_RAC_MAX_RANGE_BYTES]; ///< Storage for initial RAC buffer
    uint8_t buf_count;    ///< Count for initial RAC buffer
    int state;            ///< The section of the file the parser is in currently.
    unsigned int segment; ///< The "segment" the code is supposed to jump to
    int i;                ///< A generic iterator used to save states between for loops.
    // Primary Header     
    uint8_t ia;           ///< Is image interlaced or/and animated or not
    uint32_t bpc;         ///< 2 ^ Bytes per channel
    uint8_t channels;     ///< Number of channels
    uint8_t varint;       ///< Number of varints to process in sequence
                          
    // Secondary Header
    
    uint8_t alphazero;    ///< Alphazero Flag
    uint8_t custombc;     ///< Custom Bitchance Flag
    uint8_t customalpha;  ///< Custom alphadiv & cutoff flag

    uint8_t cut;          ///< Chancetable custom cutoff
    uint8_t alpha;        ///< Chancetable custom alphadivisor
    uint8_t ipp;          ///< Invisible pixel predictor

    uint8_t loops;        ///< Number of times animation loops
    uint16_t *framedelay; ///< Frame delay for each frame

    // Transforms
    // Size dynamically maybe
    FLIF16TransformContext *transforms[13];
    uint8_t transform_top;
    FLIF16RangesContext *range; ///< The minimum and maximum values a
                                ///  channel's pixels can take. Changes
                                ///  depending on transformations applied
    FLIF16RangesContext *prev_range;

    // MANIAC Trees and pixeldata
    int32_t (*prop_ranges)[2]; ///< Property Ranges
    uint32_t prop_ranges_size;


    // Image Properties
    /*
     * 3 output pixel formats are to be supported:
     *     1. greyscale  AV_PIX_FMT_GRAY16LE 
     *     2. RGB        AV_PIX_FMT_RGB
     *     3. RGBA       AV_PIX_FMT_RGBA
     *
     * see libavutil/pixfmt.h, libavutil/pixdesc.c
     */

    // Dimensions and other things.
    uint32_t width;
    uint32_t height;
    uint32_t frames;
    uint32_t meta;      ///< Size of a meta chunk
} FLIF16DecoderContext;

int32_t (*ff_flif16_maniac_ni_prop_ranges_init(unsigned int *prop_ranges_size,
                                            FLIF16RangesContext *ranges,
                                            uint8_t property,
                                            uint8_t channels))[2];

int ff_flif16_frames_init(FLIF16PixelData *frames,
                          uint32_t num_frames, uint8_t num_planes,
                          uint32_t depth, uint32_t width, uint32_t height);

void ff_flif16_frames_free(FLIF16PixelData *frames, uint32_t num_frames,
                           uint32_t num_planes);

static inline void ff_flif16_pixel_set(FLIF16PixelData *frame, uint8_t plane,
                                       uint32_t row, uint32_t col,
                                       FLIF16ColorVal value)
{
    frame->data[plane][image->height * row + col] = value;
}

static inline FLIF16ColorVal ff_flif16_pixel_get(FLIF16PixelData *frame, uint8_t plane,
                                                 uint32_t row, uint32_t col)
{
    if(frame->is_constant[plane])
        return (FLIF16ColorVal) *frame->data
    else
        return (FLIF16ColorVal) *frame->data[plane][image->height * row + col];
}

static inline void ff_flif16_copy_rows(FLIF16PixelData *dest,
                                       FLIF16PixelData *src, uint8_t plane,
                                       uint32_t row, uint32_t col_start,
                                       uint32_t col_end)
{
    for(uint32_t col = col_start; col < col_end; ++col)
        ff_flif16_pixel_set(dest, plane, row, col, ff_flif16_pixel_get(src, plane, row, col));
}


#define MEDIAN3(a, b, c) (((a) < (b)) ? (((b) < (c)) ? (b) : ((a) < (c) ? (c) : (a))) : (((a) < (c)) ? (a) : ((b) < (c) ? (c) : (b))))

// Must be included here to resolve circular include
#include "flif16_transform.h"

#endif /* AVCODEC_FLIF16_H */
