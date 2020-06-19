/*
 * FLIF16 Image Format Definitions
 * Copyright (c) 2020 AKartik K. Khullar <kartikkhullar840@gmail.com>
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
 * FLIF16 pixel format.
 */

#include "flif16.h"

#define SCALED(x) ((x==0) ? 0 : ((((x)-1)>>scale)+1))

typedef struct PlaneContext{
    FLIF16ColorVal* data;
    size_t height, width;
    int s;
    size_t s_r, s_c;
}PlaneContext;

typedef struct Plane{
    void (*init)(PlaneContext *, size_t, size_t, int);
    void (*clear)(PlaneContext *);
    void (*set)(PlaneContext *, const size_t, const size_t, FLIF16ColorVal);
    FLIF16ColorVal (*get)(PlaneContext *, const size_t, const size_t);
    void (*prepare_zoomlevel)(PlaneContext *, const int);
    FLIF16ColorVal (*get_fast)(PlaneContext *, size_t, size_t);
    void (*set_fast)(PlaneContext *, size_t, size_t, FLIF16ColorVal);
}Plane;

//These will be used in interlacing mode only.
static size_t zoom_rowpixelsize(int zoomlevel){
    return 1<<((zoomlevel+1)/2);
}

static size_t zoom_colpixelsize(int zoomlevel){
    return 1<<((zoomlevel)/2);
}

inline void ff_plane_init(PlaneContext *plane, size_t w, size_t h, size_t scale){
    plane->height = SCALED(h);
    plane->width = SCALED(w);
    plane->s = scale;
    plane->data = (FLIF16ColorVal *)av_mallocz(h*w*sizeof(FLIF16ColorVal));
}

inline void ff_plane_clear(PlaneContext *plane){
    av_freep(&plane->data);
}

inline void ff_plane_set(PlaneContext *plane, const size_t r, const size_t c, FLIF16ColorVal x){
    assert(r < plane->height);
    assert(c < plane->width);
    plane->data[r*plane->width + c] = x;
}

inline FLIF16ColorVal ff_plane_get(PlaneContext *plane, const size_t r, const size_t c){
    assert(r < plane->height);
    assert(c < plane->width);
    return plane->data[r*plane->width + c];
}

inline void ff_plane_prepare_zoomlevel(PlaneContext *plane, const int z){
    plane->s_r = (zoom_rowpixelsize(z)>>plane->s)*plane->width;
    plane->s_c = (zoom_rowpixelsize(z)>>plane->s);
}

inline FLIF16ColorVal ff_plane_get_fast(PlaneContext *plane, size_t r, size_t c){
    return plane->data[r*plane->s_r + c*plane->s_c];
}

inline void ff_plane_set_fast(PlaneContext *plane, size_t r, size_t c, FLIF16ColorVal x){
    plane->data[r*plane->s_r + c*plane->s_c] = x;
}