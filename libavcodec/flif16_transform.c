/*
 * Transforms for FLIF16
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
 * Transforms for FLIF16
 */

#include "flif16_transform.h"
#include "flif16_rangecoder.h"
#include "libavutil/common.h"

// Transform private structs

typedef struct TransformPrivYCoCg {
    FLIF16RangesContext *r_ctx;
    int origmax4;
} TransformPrivYCoCg;

typedef struct TransformPrivPermuteplanes {
    uint8_t subtract;
    uint8_t permutation[5];

    FLIF16RangesContext *r_ctx;
    uint8_t from[4], to[4];
    FLIF16ChanceContext ctx_a;
} TransformPrivPermuteplanes;

typedef struct TransformPrivChannelcompact {
    FLIF16ChanceContext ctx_a;
    size_t cpalette_size[4];
    FLIF16ColorVal *cpalette[4];
    FLIF16ColorVal *cpalette_inv[4];
    FLIF16ColorVal min;
    unsigned int cpalette_inv_size[4];
    int remaining;
    unsigned int i; // Iterator for nested loop.
} TransformPrivChannelcompact;

typedef struct TransformPrivBounds {
    FLIF16ChanceContext ctx_a;
    FLIF16ColorVal (*bounds)[2];
    int min;
} TransformPrivBounds;

typedef struct TransformPrivPalette {
    FLIF16ChanceContext ctx;
    FLIF16ChanceContext ctx_y;
    FLIF16ChanceContext ctx_i;
    FLIF16ChanceContext ctx_q;
    FLIF16ColorVal (*palette)[3];
    FLIF16ColorVal min[3], max[3];
    FLIF16ColorVal *prev;
    FLIF16ColorVal pp[2];
    FLIF16ColorVal y, i, q;
    long unsigned size;
    unsigned int p; // Iterator
    int32_t max_palette_size;
    uint8_t has_alpha;
    uint8_t ordered_palette;
    uint8_t sorted;
} TransformPrivPalette;

typedef struct TransformPrivPalettealpha {
    FLIF16ChanceContext ctx;
    FLIF16ChanceContext ctx_y;
    FLIF16ChanceContext ctx_i;
    FLIF16ChanceContext ctx_q;
    FLIF16ChanceContext ctx_a;
    FLIF16ColorVal (*palette)[4];
    FLIF16ColorVal min[4], max[4];
    FLIF16ColorVal *prev;
    FLIF16ColorVal pp[2];
    FLIF16ColorVal y, i, q, a;
    long unsigned int size;
    unsigned int max_palette_size;
    unsigned int p;
    uint8_t alpha_zero_special;
    uint8_t ordered_palette;
    uint8_t already_has_palette;
    uint8_t sorted;
} TransformPrivPalettealpha;

typedef int16_t ColorValCB;
typedef struct ColorValCB_list ColorValCB_list ;

typedef struct ColorValCB_list {
    ColorValCB data;
    ColorValCB_list *next;
} ColorValCB_list;

typedef struct ColorBucket {
    ColorValCB *snapvalues;
    ColorValCB_list *values;
    ColorValCB_list *values_last;
    ColorValCB min, max;
    unsigned int snapvalues_size;
    unsigned int values_size;
    uint8_t discrete;
} ColorBucket;

typedef struct ColorBuckets {
    ColorBucket bucket0;
    ColorBucket bucket3;
    ColorBucket empty_bucket;
    ColorBucket *bucket1;
    ColorBucket **bucket2; // List of a list
    FLIF16RangesContext *ranges;
    unsigned int bucket1_size;
    unsigned int bucket2_size, bucket2_list_size;
    int min0, min1;

    /*
     *  Data members used while reading buckets
     */
    unsigned int i, i2; // Iterator
    FLIF16ColorVal smin, smax;
    FLIF16ColorVal v;
    int nb;
} ColorBuckets;

typedef struct TransformPrivColorbuckets {
    FLIF16ChanceContext ctx[6];
    ColorBuckets *cb;
    FLIF16ColorVal pixel_l[2], pixel_u[2];
    int i, j, k; // Iterators
    uint8_t really_used;
} TransformPrivColorbuckets;

typedef struct TransformPrivFramedup {
    FLIF16ChanceContext chancectx;
    int *seen_before;
    unsigned int i;
    uint32_t nb;
} TransformPrivFramedup;

typedef struct TransformPrivFrameshape {
    FLIF16ChanceContext chancectx;
    int *b, *e; // Begin and end
    uint32_t cols;
    uint32_t nb;
    unsigned int i;
} TransformPrivFrameshape;

typedef struct TransformPrivFramecombine {
    FLIF16ChanceContext chancectx;
    int max_lookback;
    int user_max_lookback;
    int nb_frames;
    uint8_t was_flat;
    uint8_t was_greyscale;
    uint8_t orig_num_planes;
} TransformPrivFramecombine;

// Ranges private structs

typedef struct RangesPrivChannelcompact {
    int nb_colors[4];
} RangesPrivChannelcompact;

typedef struct RangesPrivYCoCg {
    FLIF16RangesContext *r_ctx;
    int origmax4;
} RangesPrivYCoCg;

typedef struct RangesPrivPermuteplanes {
    FLIF16RangesContext *r_ctx;
    uint8_t permutation[5];
} RangesPrivPermuteplanes;

typedef struct RangesPrivBounds {
    FLIF16ColorVal (*bounds)[2];
    FLIF16RangesContext *r_ctx;
} RangesPrivBounds;

typedef struct RangesPrivPalette {
    FLIF16RangesContext *r_ctx;
    int nb_colors;
} RangesPrivPalette;

typedef struct RangesPrivColorbuckets {
    FLIF16RangesContext *r_ctx;
    ColorBuckets *buckets;
} RangesPrivColorbuckets;

typedef struct RangesPrivFramecombine {
    FLIF16RangesContext *ranges;
    FLIF16ColorVal num_prev_frames;
    FLIF16ColorVal alpha_min;
    FLIF16ColorVal alpha_max;
} RangesPrivFramecombine;

typedef struct RangesPrivStatic {
    FLIF16ColorVal (*bounds)[2];
} RangesPrivStatic;

/*
 * =============================================================================
 * Ranges
 * =============================================================================
 */

/*
 * Static
 */

static FLIF16ColorVal ff_static_min(FLIF16RangesContext *r_ctx, int p)
{
    RangesPrivStatic *data = r_ctx->priv_data;
    av_assert1(p < r_ctx->num_planes);
    return data->bounds[p][0];
}

static FLIF16ColorVal ff_static_max(FLIF16RangesContext *r_ctx, int p)
{
    RangesPrivStatic *data = r_ctx->priv_data;
    av_assert1(p < r_ctx->num_planes);
    return data->bounds[p][1];
}

static void ff_static_minmax(FLIF16RangesContext *src_ctx ,const int p,
                             FLIF16ColorVal *prev_planes,
                             FLIF16ColorVal *minv, FLIF16ColorVal *maxv)
{
    *minv = ff_flif16_ranges_min(src_ctx, p);
    *maxv = ff_flif16_ranges_max(src_ctx, p);
}

static void ff_static_snap(FLIF16RangesContext *src_ctx , const int p,
                           FLIF16ColorVal *prev_planes,
                           FLIF16ColorVal *minv, FLIF16ColorVal *maxv,
                           FLIF16ColorVal *v)
{
    ff_flif16_ranges_minmax(src_ctx, p, prev_planes, minv, maxv);
    *maxv = FFMAX(*minv, *maxv);
    *v = av_clip(*v, *minv, *maxv);
}

static void ff_static_close(FLIF16RangesContext *r_ctx)
{
    RangesPrivStatic *data = r_ctx->priv_data;
    av_free(data->bounds);
}

/*
 * ChannelCompact
 */

static FLIF16ColorVal ff_channelcompact_min(FLIF16RangesContext *ranges, int p)
{
    return 0;
}

static FLIF16ColorVal ff_channelcompact_max(FLIF16RangesContext *src_ctx, int p)
{
    RangesPrivChannelcompact *data = src_ctx->priv_data;
    return data->nb_colors[p];
}

static void ff_channelcompact_minmax(FLIF16RangesContext *r_ctx, int p,
                                     FLIF16ColorVal *prev_planes,
                                     FLIF16ColorVal *minv, FLIF16ColorVal *maxv)
{
    RangesPrivChannelcompact *data = r_ctx->priv_data;
    *minv = 0;
    *maxv = data->nb_colors[p];
}

/*
 * YCoCg
 */

static inline FLIF16ColorVal ff_get_max_y(int origmax4)
{
    return 4 * origmax4 - 1;
}

static inline int ff_get_min_co(int origmax4, int yval)
{
    if (yval < origmax4 - 1)
        return (-3) - (4 * yval);
    else if (yval >= 3 * origmax4)
        return 4 * (1 + yval - (4 * origmax4));
    else
        return (-4) * origmax4 + 1;
}

static inline int ff_get_max_co(int origmax4, int yval)
{
    if (yval < origmax4 - 1)
        return 3 + 4 * yval;
    else if (yval >= 3 * origmax4)
        return 4 * origmax4 - 4 * (1 + yval - 3 * origmax4);
    else
        return 4 * origmax4 - 1;
}

static inline int ff_get_min_cg(int origmax4, int yval, int coval)
{
    if (yval < origmax4 - 1)
        return -(2 * yval + 1);
    else if (yval >= 3 * origmax4)
        return -(2 * (4 * origmax4 - 1 - yval) - ((1 + abs(coval)) / 2) * 2);
    else {
        return -FFMIN(2 * origmax4 - 1 + (yval - origmax4 + 1) * 2,
                      2 * origmax4 + (3 * origmax4 - 1 - yval) * 2 - ((1 + abs(coval)) / 2)*2);
    }
}

static inline int ff_get_max_cg(int origmax4, int yval, int coval)
{
    if (yval < origmax4 - 1)
        return 1 + 2 * yval - 2 * (abs(coval) / 2);
    else if (yval >= 3 * origmax4)
        return 2 * (4*origmax4 - 1 - yval);
    else
        return -FFMAX(- 4 * origmax4 + (1 + yval - 2 * origmax4) * 2,
                      - 2 * origmax4 - (yval - origmax4) * 2 - 1 + (abs(coval) / 2) * 2);
}

static FLIF16ColorVal ff_ycocg_min(FLIF16RangesContext *r_ctx, int p)
{
    RangesPrivYCoCg *data = r_ctx->priv_data;
    switch (p) {
    case FLIF16_PLANE_Y:
        return 0;
    case FLIF16_PLANE_CO:
    case FLIF16_PLANE_CG:
        return -4 * data->origmax4 + 1;
    default:
        return ff_flif16_ranges_min(data->r_ctx, p);
    }
}

static FLIF16ColorVal ff_ycocg_max(FLIF16RangesContext *r_ctx, int p)
{
    RangesPrivYCoCg *data = r_ctx->priv_data;
    switch (p) {
    case FLIF16_PLANE_Y:
    case FLIF16_PLANE_CO:
    case FLIF16_PLANE_CG:
        return 4 * data->origmax4 - 1;
    default:
        return ff_flif16_ranges_max(data->r_ctx, p);
    }
}

static void ff_ycocg_minmax(FLIF16RangesContext *r_ctx ,const int p,
                            FLIF16ColorVal *prev_planes,
                            FLIF16ColorVal *minv, FLIF16ColorVal *maxv)
{
    RangesPrivYCoCg *data = r_ctx->priv_data;
    switch (p) {
    case FLIF16_PLANE_Y:
        *minv = 0;
        *maxv = ff_get_max_y(data->origmax4);
        break;
    case FLIF16_PLANE_CO:
        *minv = ff_get_min_co(data->origmax4, prev_planes[0]);
        *maxv = ff_get_max_co(data->origmax4, prev_planes[0]);
        break;
    case FLIF16_PLANE_CG:
        *minv = ff_get_min_cg(data->origmax4, prev_planes[0], prev_planes[1]);
        *maxv = ff_get_max_cg(data->origmax4, prev_planes[0], prev_planes[1]);
        break;
    default:
        ff_flif16_ranges_minmax(data->r_ctx, p, prev_planes, minv, maxv);
    }
}

static void ff_ycocg_close(FLIF16RangesContext *r_ctx)
{
    RangesPrivYCoCg *data = r_ctx->priv_data;
    const FLIF16Ranges *range = flif16_ranges[data->r_ctx->r_no];
    if (range->close)
        range->close(data->r_ctx);
    av_freep(&data->r_ctx->priv_data);
    av_freep(&data->r_ctx);
}

/*
 * PermutePlanesSubtract
 */

static FLIF16ColorVal ff_permuteplanessubtract_min(FLIF16RangesContext *r_ctx, int p)
{
    RangesPrivPermuteplanes *data = r_ctx->priv_data;
    if (p == 0 || p > 2)
        return ff_flif16_ranges_min(data->r_ctx, data->permutation[p]);
    return ff_flif16_ranges_min(data->r_ctx, data->permutation[p]) -
           ff_flif16_ranges_max(data->r_ctx, data->permutation[0]);
}

static FLIF16ColorVal ff_permuteplanessubtract_max(FLIF16RangesContext *r_ctx, int p)
{
    RangesPrivPermuteplanes *data = r_ctx->priv_data;
    if (p == 0 || p > 2)
        return ff_flif16_ranges_max(data->r_ctx, data->permutation[p]);
    return ff_flif16_ranges_max(data->r_ctx, data->permutation[p]) -
           ff_flif16_ranges_min(data->r_ctx, data->permutation[0]);
}

static void ff_permuteplanessubtract_minmax(FLIF16RangesContext *r_ctx, int p,
                                            FLIF16ColorVal *prev_planes,
                                            FLIF16ColorVal *minv,
                                            FLIF16ColorVal *maxv)
{
    RangesPrivPermuteplanes *data = r_ctx->priv_data;
    if (p == 0 || p > 2) {
        *minv = ff_flif16_ranges_min(data->r_ctx, p);
        *maxv = ff_flif16_ranges_max(data->r_ctx, p);
    } else {
        *minv = ff_flif16_ranges_min(data->r_ctx, data->permutation[p]) - prev_planes[0];
        *maxv = ff_flif16_ranges_max(data->r_ctx, data->permutation[p]) - prev_planes[0];
    }
}

/*
 * PermutePlanes
 */

static FLIF16ColorVal ff_permuteplanes_min(FLIF16RangesContext *r_ctx, int p)
{
    RangesPrivPermuteplanes *data = r_ctx->priv_data;
    return ff_flif16_ranges_min(data->r_ctx, data->permutation[p]);
}

static FLIF16ColorVal ff_permuteplanes_max(FLIF16RangesContext *r_ctx, int p)
{
    RangesPrivPermuteplanes *data = r_ctx->priv_data;
    return ff_flif16_ranges_max(data->r_ctx, data->permutation[p]);
}

static void ff_permuteplanes_close(FLIF16RangesContext *r_ctx)
{
    RangesPrivPermuteplanes *data = r_ctx->priv_data;
    const FLIF16Ranges *range = flif16_ranges[data->r_ctx->r_no];
    if (range->close)
        range->close(data->r_ctx);
    av_freep(&data->r_ctx->priv_data);
    av_freep(&data->r_ctx);
}

/*
 * Bounds
 */

static FLIF16ColorVal ff_bounds_min(FLIF16RangesContext *r_ctx, int p)
{
    RangesPrivBounds *data = r_ctx->priv_data;
    av_assert1(p < r_ctx->num_planes);
    return FFMAX(ff_flif16_ranges_min(data->r_ctx, p), data->bounds[p][0]);
}

static FLIF16ColorVal ff_bounds_max(FLIF16RangesContext *r_ctx, int p)
{
    RangesPrivBounds *data = r_ctx->priv_data;
    av_assert1(p < r_ctx->num_planes);
    return FFMIN(ff_flif16_ranges_max(data->r_ctx, p), data->bounds[p][1]);
}

static void ff_bounds_minmax(FLIF16RangesContext *r_ctx, int p,
                             FLIF16ColorVal *prev_planes,
                             FLIF16ColorVal *minv, FLIF16ColorVal *maxv)
{
    RangesPrivBounds *data = r_ctx->priv_data;
    av_assert1(p < r_ctx->num_planes);
    if (p == 0 || p == 3) {
        *minv = data->bounds[p][0];
        *maxv = data->bounds[p][1];
        return;
    }
    ff_flif16_ranges_minmax(data->r_ctx, p, prev_planes, minv, maxv);
    *minv = FFMAX(*minv, data->bounds[p][0]);
    *maxv = FFMIN(*maxv, data->bounds[p][1]);
    if (*minv > *maxv) {
        *minv = data->bounds[p][0];
        *maxv = data->bounds[p][1];
    }
    av_assert1(*minv <= *maxv);
}

static void ff_bounds_snap(FLIF16RangesContext *r_ctx, int p,
                           FLIF16ColorVal *prev_planes, FLIF16ColorVal *minv,
                           FLIF16ColorVal *maxv, FLIF16ColorVal *v)
{
    RangesPrivBounds *data = r_ctx->priv_data;
    if (p == 0 || p == 3) {
        *minv = data->bounds[p][0];
        *maxv = data->bounds[p][1];
    } else {
        ff_flif16_ranges_snap(data->r_ctx, p, prev_planes, minv, maxv, v);
        *minv = FFMAX(*minv, data->bounds[p][0]);
        *maxv = FFMIN(*maxv, data->bounds[p][1]);
        if (*minv > *maxv) {
            *minv = data->bounds[p][0];
            *maxv = data->bounds[p][1];
        }
    }
    *v = av_clip(*v, *minv, *maxv);
}

static void ff_bounds_close(FLIF16RangesContext *r_ctx)
{
    RangesPrivBounds *data = r_ctx->priv_data;
    const FLIF16Ranges *range = flif16_ranges[data->r_ctx->r_no];
    if (range->close)
        range->close(data->r_ctx);
    av_free(data->r_ctx->priv_data);
    av_free(data->bounds);
    av_free(data->r_ctx);
}

/*
 * Palette
 */

static FLIF16ColorVal ff_palette_min(FLIF16RangesContext *r_ctx, int p)
{
    RangesPrivPalette *data = r_ctx->priv_data;
    if (p < 3)
        return 0;
    else
        return ff_flif16_ranges_min(data->r_ctx, p);
}

static FLIF16ColorVal ff_palette_max(FLIF16RangesContext *r_ctx, int p)
{
    RangesPrivPalette *data = r_ctx->priv_data;
    if (p == 1)
        return data->nb_colors-1;
    else if (p < 3)
        return 0;
    else
        return ff_flif16_ranges_max(data->r_ctx, p);
}

static void ff_palette_minmax(FLIF16RangesContext *r_ctx, int p,
                              FLIF16ColorVal *prev_planes,
                              FLIF16ColorVal *minv, FLIF16ColorVal *maxv)
{
    RangesPrivPalette *data = r_ctx->priv_data;
    if (p == FLIF16_PLANE_CO) {
        *minv = 0;
        *maxv = data->nb_colors-1;
    } else if (p < FLIF16_PLANE_ALPHA) {
        *minv = 0;
        *maxv = 0;
    } else
        ff_flif16_ranges_minmax(data->r_ctx, p, prev_planes, minv, maxv);
}

static void ff_palette_close(FLIF16RangesContext *r_ctx)
{
    RangesPrivPalette *data = r_ctx->priv_data;
    const FLIF16Ranges *range = flif16_ranges[data->r_ctx->r_no];
    if (range->close)
        range->close(data->r_ctx);
    av_freep(&data->r_ctx->priv_data);
    av_freep(&data->r_ctx);
}

/*
 * Palette Alpha
 */

static FLIF16ColorVal ff_palettealpha_min(FLIF16RangesContext *r_ctx, int p)
{
    RangesPrivPalette *data = r_ctx->priv_data;
    if (p < FLIF16_PLANE_ALPHA)
        return 0;
    else if (p == FLIF16_PLANE_ALPHA)
        return 1;
    else
        return ff_flif16_ranges_min(data->r_ctx, p);
}

static FLIF16ColorVal ff_palettealpha_max(FLIF16RangesContext *r_ctx, int p)
{
    RangesPrivPalette *data = r_ctx->priv_data;
    switch (p) {
    case FLIF16_PLANE_Y:
        return 0;
    case FLIF16_PLANE_CO:
        return data->nb_colors-1;
    case FLIF16_PLANE_CG:
        return 0;
    case FLIF16_PLANE_ALPHA:
        return 1;
    default:
        return ff_flif16_ranges_max(data->r_ctx, p);
    }
}

static void ff_palettealpha_minmax(FLIF16RangesContext *r_ctx, int p,
                                   FLIF16ColorVal *prev_planes,
                                   FLIF16ColorVal *minv, FLIF16ColorVal *maxv)
{
    RangesPrivPalette *data = r_ctx->priv_data;
    if (p == FLIF16_PLANE_CO) {
        *minv = 0;
        *maxv = data->nb_colors - 1;
    } else if (p < FLIF16_PLANE_ALPHA) {
        *minv = 0;
        *maxv = 0;
    } else if (p == FLIF16_PLANE_ALPHA) {
        *minv = 1;
        *maxv = 1;
    } else
        ff_flif16_ranges_minmax(data->r_ctx, p, prev_planes, minv, maxv);
}

/*
 * ColorBuckets
 */

static void ff_init_bucket_default(ColorBucket *b)
{
    b->min = 10000;
    b->max = -10000;
    b->discrete = 1;
    b->values_size = 0;
    b->snapvalues_size = 0;
    b->snapvalues = NULL;
    b->values = NULL;
}

static ColorBucket *ff_bucket_buckets2(ColorBuckets *buckets, const int p,
                                       const FLIF16ColorVal *prev_planes)
{
    unsigned diff = prev_planes[0] - buckets->min0;
    unsigned diff1;
    av_assert0(p >= FLIF16_PLANE_Y);
    av_assert0(p < FLIF16_PLANE_LOOKBACK);
    if (p == FLIF16_PLANE_Y)
        return &buckets->bucket0;
    if (p == FLIF16_PLANE_CO) {
        av_assert0(diff < buckets->bucket1_size);
        return &buckets->bucket1[diff];
    }
    if (p == FLIF16_PLANE_CG) {
        diff1 = (prev_planes[1] - buckets->min1) / 4;
        av_assert0(diff < buckets->bucket2_size);
        av_assert0(diff1 < buckets->bucket2_list_size);
        return &buckets->bucket2[diff][diff1];
    }

    return &buckets->bucket3;
}

static ColorBucket *ff_bucket_buckets(ColorBuckets *buckets, const int p,
                                      const FLIF16ColorVal *prev_planes)
{
    av_assert0(p >= 0);
    av_assert0(p < 4);
    if (p == FLIF16_PLANE_Y)
        return &buckets->bucket0;
    if (p == FLIF16_PLANE_CO) {
        int i = (prev_planes[0] - buckets->min0);
        // i is signed because following check is necessary for code flow.
        if (i >= 0 && i < (int)buckets->bucket1_size)
            return &buckets->bucket1[i];
        else
            return &buckets->empty_bucket;
    }
    if (p == FLIF16_PLANE_CG) {
        int i = (prev_planes[0] - buckets->min0);
        int j = (prev_planes[1] - buckets->min1) / 4;
        if (i >= 0 && i < (int)buckets->bucket1_size &&
            j >= 0 && j < (int) buckets->bucket2_list_size)
            return &buckets->bucket2[i][j];
        else
            return &buckets->empty_bucket;
    }

    return &buckets->bucket3;
}

static FLIF16ColorVal ff_snap_color_bucket(ColorBucket *bucket, FLIF16ColorVal c)
{
    if (c <= bucket->min) {
        return bucket->min;
    }
    if (c >= bucket->max) {
        return bucket->max;
    }
    if (bucket->discrete) {
        av_assert1((FLIF16ColorVal)bucket->snapvalues_size > (c - bucket->min));
        return bucket->snapvalues[c - bucket->min];
    }
    return c;
}

static FLIF16ColorVal ff_colorbuckets_min(FLIF16RangesContext *r_ctx, int p)
{
    RangesPrivColorbuckets *data = r_ctx->priv_data;
    return ff_flif16_ranges_min(data->r_ctx, p);
}

static FLIF16ColorVal ff_colorbuckets_max(FLIF16RangesContext *r_ctx, int p)
{
    RangesPrivColorbuckets *data = r_ctx->priv_data;
    return ff_flif16_ranges_max(data->r_ctx, p);
}

static void ff_colorbuckets_snap(FLIF16RangesContext *src_ctx, const int p,
                                 FLIF16ColorVal *prev_planes,
                                 FLIF16ColorVal *minv, FLIF16ColorVal *maxv,
                                 FLIF16ColorVal *v)
{
    RangesPrivColorbuckets *data = src_ctx->priv_data;
    ColorBucket *b = ff_bucket_buckets(data->buckets, p, prev_planes);
    *minv = b->min;
    *maxv = b->max;
    if (b->min > b->max) {
        *minv = ff_colorbuckets_min(src_ctx, p);
        *v = *minv;
        *maxv = ff_colorbuckets_max(src_ctx, p);
        return;
    }
    *v = ff_snap_color_bucket(b, *v);
}

static void ff_colorbuckets_minmax(FLIF16RangesContext *r_ctx,
                                   int p, FLIF16ColorVal *prev_planes,
                                   FLIF16ColorVal *minv, FLIF16ColorVal *maxv)
{
    RangesPrivColorbuckets *data = r_ctx->priv_data;
    const ColorBucket *b = ff_bucket_buckets(data->buckets, p, prev_planes);
    *minv = b->min;
    *maxv = b->max;
    if (b->min > b->max) {
        *minv = ff_colorbuckets_min(r_ctx, p);
        *maxv = ff_colorbuckets_max(r_ctx, p);
    }
}

static void ff_list_close(ColorValCB_list *list)
{
    ColorValCB_list *temp;
    while (list) {
        temp = list;
        list = list->next;
        av_freep(&temp);
    }
}

static void ff_priv_colorbuckets_close(ColorBuckets *cb)
{
    for (unsigned int i = 0; i < cb->bucket1_size; i++) {
        if (cb->bucket1[i].snapvalues)
            av_freep(&cb->bucket1[i].snapvalues);
        if (cb->bucket1[i].values)
            ff_list_close(cb->bucket1[i].values);
    }
    av_freep(&cb->bucket1);

    if (cb->bucket0.snapvalues)
        av_freep(&cb->bucket0.snapvalues);
    if (cb->bucket0.values)
        ff_list_close(cb->bucket0.values);

    if (cb->bucket3.snapvalues)
        av_freep(&cb->bucket3.snapvalues);
    if (cb->bucket3.values)
        ff_list_close(cb->bucket3.values);

    for (unsigned int i = 0; i < cb->bucket2_size; i++) {
        for (unsigned int j = 0; j < cb->bucket2_list_size; j++) {
            if (cb->bucket2[i][j].snapvalues)
                av_freep(&cb->bucket2[i][j].snapvalues);
            if (cb->bucket2[i][j].values)
                ff_list_close(cb->bucket2[i][j].values);
        }
        av_freep(&cb->bucket2[i]);
    }
    av_freep(&cb->bucket2);
}

static void ff_colorbuckets_close(FLIF16RangesContext *r_ctx)
{
    RangesPrivColorbuckets *data = r_ctx->priv_data;
    const FLIF16Ranges *range = flif16_ranges[data->r_ctx->r_no];
    if (range->close)
        range->close(data->r_ctx);
    av_freep(&data->r_ctx->priv_data);
    av_freep(&data->r_ctx);
}

static FLIF16ColorVal ff_framecombine_min(FLIF16RangesContext *r_ctx, int p)
{
    RangesPrivFramecombine *data = r_ctx->priv_data;
    if (p < FLIF16_PLANE_ALPHA)
        return ff_flif16_ranges_min(data->ranges, p);
    else if (p == FLIF16_PLANE_ALPHA)
        return data->alpha_min;
    else
        return 0;
}

static FLIF16ColorVal ff_framecombine_max(FLIF16RangesContext *r_ctx, int p)
{
    RangesPrivFramecombine *data = r_ctx->priv_data;
    if (p < FLIF16_PLANE_ALPHA)
        return ff_flif16_ranges_max(data->ranges, p);
    else if (p == FLIF16_PLANE_ALPHA)
        return data->alpha_max;
    else
        return data->num_prev_frames;
}

static void ff_framecombine_minmax(FLIF16RangesContext *r_ctx,
                                   int p, FLIF16ColorVal *prev_planes,
                                   FLIF16ColorVal *minv, FLIF16ColorVal *maxv)
{
    RangesPrivFramecombine *data = r_ctx->priv_data;
    if (p >= 3) {
        *minv = ff_framecombine_min(r_ctx, p);
        *maxv = ff_framecombine_max(r_ctx, p);
    } else
        ff_flif16_ranges_minmax(data->ranges, p, prev_planes, minv, maxv);
}

static void ff_framecombine_snap(FLIF16RangesContext *src_ctx, const int p,
                                 FLIF16ColorVal *prev_planes,
                                 FLIF16ColorVal *minv, FLIF16ColorVal *maxv,
                                 FLIF16ColorVal *v)
{
    RangesPrivFramecombine *data = src_ctx->priv_data;
    if (p >= 3)
        ff_static_snap(src_ctx, p, prev_planes, minv, maxv, v);
    else
        ff_flif16_ranges_snap(data->ranges, p, prev_planes, minv, maxv, v);
}

static void ff_framecombine_close(FLIF16RangesContext *r_ctx)
{
    RangesPrivFramecombine *data = r_ctx->priv_data;
    const FLIF16Ranges *range = flif16_ranges[data->ranges->r_no];
    if (range->close) {
        range->close(data->ranges);
        av_freep(&data->ranges->priv_data);
    }
    av_freep(&data->ranges);
}

static const FLIF16Ranges flif16_ranges_static = {
    .priv_data_size = sizeof(RangesPrivStatic),
    .min            = &ff_static_min,
    .max            = &ff_static_max,
    .minmax         = &ff_static_minmax,
    .snap           = &ff_static_snap,
    .is_static      = 1,
    .close          = &ff_static_close
};

static const FLIF16Ranges flif16_ranges_channelcompact = {
    .priv_data_size = sizeof(RangesPrivChannelcompact),
    .min            = &ff_channelcompact_min,
    .max            = &ff_channelcompact_max,
    .minmax         = &ff_channelcompact_minmax,
    .snap           = &ff_static_snap,
    .is_static      = 1,
    .close          = NULL
};

static const FLIF16Ranges flif16_ranges_ycocg = {
    .priv_data_size = sizeof(RangesPrivYCoCg),
    .min            = &ff_ycocg_min,
    .max            = &ff_ycocg_max,
    .minmax         = &ff_ycocg_minmax,
    .snap           = &ff_static_snap,
    .is_static      = 0,
    .close          = &ff_ycocg_close
};

static const FLIF16Ranges flif16_ranges_permuteplanessubtract = {
    .priv_data_size = sizeof(RangesPrivPermuteplanes),
    .min            = &ff_permuteplanessubtract_min,
    .max            = &ff_permuteplanessubtract_max,
    .minmax         = &ff_permuteplanessubtract_minmax,
    .snap           = &ff_static_snap,
    .is_static      = 0,
    .close          = &ff_permuteplanes_close
};

static const FLIF16Ranges flif16_ranges_permuteplanes = {
    .priv_data_size = sizeof(RangesPrivPermuteplanes),
    .min            = &ff_permuteplanes_min,
    .max            = &ff_permuteplanes_max,
    .minmax         = &ff_static_minmax,
    .snap           = &ff_static_snap,
    .is_static      = 0,
    .close          = &ff_permuteplanes_close
};

static const FLIF16Ranges flif16_ranges_bounds = {
    .priv_data_size = sizeof(RangesPrivBounds),
    .min            = &ff_bounds_min,
    .max            = &ff_bounds_max,
    .minmax         = &ff_bounds_minmax,
    .snap           = &ff_bounds_snap,
    .is_static      = 0,
    .close          = &ff_bounds_close
};

static const FLIF16Ranges flif16_ranges_palette = {
    .priv_data_size = sizeof(RangesPrivPalette),
    .min            = &ff_palette_min,
    .max            = &ff_palette_max,
    .minmax         = &ff_palette_minmax,
    .snap           = &ff_static_snap,
    .is_static      = 0,
    .close          = &ff_palette_close
};

static const FLIF16Ranges flif16_ranges_palettealpha = {
    .priv_data_size = sizeof(RangesPrivPalette),
    .min            = &ff_palettealpha_min,
    .max            = &ff_palettealpha_max,
    .minmax         = &ff_palettealpha_minmax,
    .snap           = &ff_static_snap,
    .is_static      = 0,
    .close          = &ff_palette_close
};

static const FLIF16Ranges flif16_ranges_colorbuckets = {
    .priv_data_size = sizeof(RangesPrivColorbuckets),
    .min            = &ff_colorbuckets_min,
    .max            = &ff_colorbuckets_max,
    .minmax         = &ff_colorbuckets_minmax,
    .snap           = &ff_colorbuckets_snap,
    .is_static      = 0,
    .close          = &ff_colorbuckets_close
};

static const FLIF16Ranges flif16_ranges_framecombine = {
    .priv_data_size = sizeof(RangesPrivFramecombine),
    .min            = &ff_framecombine_min,
    .max            = &ff_framecombine_max,
    .minmax         = &ff_framecombine_minmax,
    .snap           = &ff_framecombine_snap,
    .is_static      = 0,
    .close          = &ff_framecombine_close
};

const FLIF16Ranges *flif16_ranges[] = {
    [FLIF16_RANGES_PERMUTEPLANESSUBTRACT] = &flif16_ranges_permuteplanessubtract,
    [FLIF16_RANGES_CHANNELCOMPACT]        = &flif16_ranges_channelcompact,
    [FLIF16_RANGES_FRAMELOOKBACK]         = &flif16_ranges_framecombine,
    [FLIF16_RANGES_PERMUTEPLANES]         = &flif16_ranges_permuteplanes,
    [FLIF16_RANGES_COLORBUCKETS]          = &flif16_ranges_colorbuckets,
    [FLIF16_RANGES_PALETTEALPHA]          = &flif16_ranges_palettealpha,
    [FLIF16_RANGES_PALETTE]               = &flif16_ranges_palette,
    [FLIF16_RANGES_BOUNDS]                = &flif16_ranges_bounds,
    [FLIF16_RANGES_STATIC]                = &flif16_ranges_static,
    [FLIF16_RANGES_YCOCG]                 = &flif16_ranges_ycocg
};

FLIF16RangesContext *ff_flif16_ranges_static_init(uint8_t num_planes,
                                                  uint32_t bpc)
{
    const FLIF16Ranges *r = flif16_ranges[FLIF16_RANGES_STATIC];
    FLIF16RangesContext *ctx;
    RangesPrivStatic *data;
    ctx = av_mallocz(sizeof(*ctx));
    if (!ctx)
        return NULL;
    ctx->r_no       = FLIF16_RANGES_STATIC;
    ctx->num_planes = num_planes;
    ctx->priv_data  = av_mallocz(r->priv_data_size);
    if (!ctx->priv_data) {
        av_freep(&ctx);
        return NULL;
    }
    data = ctx->priv_data;
    data->bounds = av_malloc_array(num_planes, sizeof(*data->bounds));
    if (!data->bounds) {
        av_freep(&ctx);
        av_freep(&ctx->priv_data);
        return NULL;
    }
    for (unsigned int i = 0; i < num_planes; ++i) {
        data->bounds[i][0] = 0;
        data->bounds[i][1] = bpc;
    }
    return ctx;
}

void ff_flif16_ranges_close(FLIF16RangesContext* r_ctx) {
    const FLIF16Ranges* ranges = flif16_ranges[r_ctx->r_no];
    if (ranges->close)
        ranges->close(r_ctx);
    if (ranges->priv_data_size)
        av_free(r_ctx->priv_data);
    av_free(r_ctx);
}

static inline void ff_flif16_planes_get(FLIF16Context *ctx, FLIF16PixelData *frame,
                                        FLIF16ColorVal *values, uint32_t row, uint32_t col)
{
    for (int i = 0; i < 3; i++)
        values[i] = ff_flif16_pixel_get(ctx, frame, i, row, col);
}

static inline void ff_flif16_planes_set(FLIF16Context *ctx, FLIF16PixelData *frame,
                                        FLIF16ColorVal *values, uint32_t row, uint32_t col)
{
    for (int i = 0; i < 3; i++)
        ff_flif16_pixel_set(ctx, frame, i, row, col, values[i]);
}

/*
 * =============================================================================
 * Transforms
 * =============================================================================
 */

/*
 * YCoCg
 */
static int transform_ycocg_init(FLIF16TransformContext *ctx, FLIF16RangesContext *r_ctx)
{
    TransformPrivYCoCg *data = ctx->priv_data;

    if (r_ctx->num_planes < 3                                            ||
        ff_flif16_ranges_min(r_ctx, 0) == ff_flif16_ranges_max(r_ctx, 0) ||
        ff_flif16_ranges_min(r_ctx, 1) == ff_flif16_ranges_max(r_ctx, 1) ||
        ff_flif16_ranges_min(r_ctx, 2) == ff_flif16_ranges_max(r_ctx, 2) ||
        ff_flif16_ranges_min(r_ctx, 0) < 0                               ||
        ff_flif16_ranges_min(r_ctx, 1) < 0                               ||
        ff_flif16_ranges_min(r_ctx, 2) < 0)
        return 0;

    data->origmax4 = FFMAX3(ff_flif16_ranges_max(r_ctx, 0),
                            ff_flif16_ranges_max(r_ctx, 1),
                            ff_flif16_ranges_max(r_ctx, 2))/4 + 1;
    data->r_ctx = r_ctx;
    return 1;
}

static FLIF16RangesContext *transform_ycocg_meta(FLIF16Context *ctx,
                                                 FLIF16PixelData *frame,
                                                 uint32_t frame_count,
                                                 FLIF16TransformContext *t_ctx,
                                                 FLIF16RangesContext *src_ctx)
{
    FLIF16RangesContext *r_ctx;
    RangesPrivYCoCg *data;
    TransformPrivYCoCg *trans_data = t_ctx->priv_data;
    r_ctx = av_mallocz(sizeof(FLIF16RangesContext));
    if (!r_ctx)
        return NULL;
    r_ctx->r_no = FLIF16_RANGES_YCOCG;
    r_ctx->priv_data = av_mallocz(sizeof(RangesPrivYCoCg));
    if (!r_ctx->priv_data)
        return NULL;
    data = r_ctx->priv_data;

    data->origmax4 = trans_data->origmax4;
    data->r_ctx    = trans_data->r_ctx;
    r_ctx->num_planes = src_ctx->num_planes;
    return r_ctx;
}

static void transform_ycocg_reverse(FLIF16Context *ctx,
                                    FLIF16TransformContext *t_ctx,
                                    FLIF16PixelData *pixel_data,
                                    uint32_t stride_row,
                                    uint32_t stride_col)
{
    int r, c;
    FLIF16ColorVal rgb[3], ycocg[3];
    int height = ctx->height;
    int width  = ctx->width;
    TransformPrivYCoCg *data = t_ctx->priv_data;

    for (r = 0; r<height; r+=stride_row) {
        for (c = 0; c<width; c+=stride_col) {
            ff_flif16_planes_get(ctx, pixel_data, ycocg, r, c);

            rgb[1] = ycocg[0] - ((-ycocg[2]) >> 1);
            rgb[2] = ycocg[0] + ((1 - ycocg[2]) >> 1) - (ycocg[1] >> 1);
            rgb[0] = ycocg[1] + rgb[2];

            rgb[0] = av_clip(rgb[0], 0, ff_flif16_ranges_max(data->r_ctx, 0));
            rgb[1] = av_clip(rgb[1], 0, ff_flif16_ranges_max(data->r_ctx, 1));
            rgb[2] = av_clip(rgb[2], 0, ff_flif16_ranges_max(data->r_ctx, 2));

            ff_flif16_planes_set(ctx, pixel_data, rgb, r, c);
        }
    }
}

/*
 * PermutePlanes
 */

static int transform_permuteplanes_init(FLIF16TransformContext *ctx,
                                        FLIF16RangesContext *r_ctx)
{
    TransformPrivPermuteplanes *data = ctx->priv_data;
    ff_flif16_chancecontext_init(&data->ctx_a);

    if (r_ctx->num_planes     < 3 ||
        ff_flif16_ranges_min(r_ctx, 0) < 0 ||
        ff_flif16_ranges_min(r_ctx, 1) < 0 ||
        ff_flif16_ranges_min(r_ctx, 2) < 0)
        return 0;

    data->r_ctx = r_ctx;
    return 1;
}

static int transform_permuteplanes_read(FLIF16TransformContext *ctx,
                                        FLIF16Context *dec_ctx,
                                        FLIF16RangesContext *r_ctx)
{
    int p;
    TransformPrivPermuteplanes *data = ctx->priv_data;

    switch (ctx->segment) {
    case 0:
        RAC_GET(&dec_ctx->rc, &data->ctx_a, 0, 1, &data->subtract,
                FLIF16_RAC_NZ_INT);

        for (p = 0; p<4; p++) {
            data->from[p] = 0;
            data->to[p] = 0;
        }
        ctx->segment = 1;

    case 1:
        for (; ctx->i < dec_ctx->num_planes; ++ctx->i) {
            RAC_GET(&dec_ctx->rc, &data->ctx_a, 0, dec_ctx->num_planes - 1,
                    &data->permutation[ctx->i],
                    FLIF16_RAC_NZ_INT);
            data->from[ctx->i] = 1;
            data->to[ctx->i] = 1;
        }
        ctx->i = 0;

        for (p = 0; p < dec_ctx->num_planes; p++) {
            if (!data->from[p] || !data->to[p])
            return 0;
        }
    }

    ctx->segment = 0;
    return 1;

need_more_data:
    return AVERROR(EAGAIN);
}

static FLIF16RangesContext *transform_permuteplanes_meta(FLIF16Context *ctx,
                                                         FLIF16PixelData *frame,
                                                         uint32_t frame_count,
                                                         FLIF16TransformContext *t_ctx,
                                                         FLIF16RangesContext *src_ctx)
{
    int i;
    FLIF16RangesContext *r_ctx;
    TransformPrivPermuteplanes *data;
    RangesPrivPermuteplanes *priv_data;

    r_ctx = av_mallocz(sizeof(*r_ctx));
    if (!r_ctx)
        return NULL;
    data = t_ctx->priv_data;
    priv_data = av_mallocz(sizeof(*priv_data));
    if (!priv_data)
        return NULL;
    if (data->subtract)
        r_ctx->r_no = FLIF16_RANGES_PERMUTEPLANESSUBTRACT;
    else
        r_ctx->r_no = FLIF16_RANGES_PERMUTEPLANES;
    r_ctx->num_planes = src_ctx->num_planes;
    for (i = 0; i < 5; i++) {
        priv_data->permutation[i] = data->permutation[i];
    }
    priv_data->r_ctx = data->r_ctx;
    r_ctx->priv_data = priv_data;
    return r_ctx;
}

static void transform_permuteplanes_reverse(FLIF16Context *ctx,
                                            FLIF16TransformContext *t_ctx,
                                            FLIF16PixelData *frame,
                                            uint32_t stride_row,
                                            uint32_t stride_col)
{
    int p, r, c;
    FLIF16ColorVal pixel[5];
    TransformPrivPermuteplanes *data = t_ctx->priv_data;
    int height = ctx->height;
    int width  = ctx->width;
    FLIF16ColorVal val;

    for (r = 0; r < height; r += stride_row) {
        for (c = 0; c < width; c += stride_col) {
            for (p = 0; p < data->r_ctx->num_planes; p++)
                pixel[p] =  ff_flif16_pixel_get(ctx, frame, p, r, c);
            for (p = 0; p < data->r_ctx->num_planes; p++)
                ff_flif16_pixel_set(ctx, frame, data->permutation[p], r, c, pixel[p]);

            ff_flif16_pixel_set(ctx, frame, data->permutation[0], r, c, pixel[0]);
            if (!data->subtract) {
                for (p = 1; p < data->r_ctx->num_planes; p++)
                    ff_flif16_pixel_set(ctx, frame, data->permutation[p], r, c, pixel[p]);
            } else {
                for (p = 1; p < 3 && p < data->r_ctx->num_planes; p++) {
                    val = av_clip(pixel[p] + pixel[0],
                                  ff_flif16_ranges_min(data->r_ctx, data->permutation[p]),
                                  ff_flif16_ranges_max(data->r_ctx, data->permutation[p]));
                    ff_flif16_pixel_set(ctx, frame, data->permutation[p], r, c, val);
                }
                for (p = 3; p < data->r_ctx->num_planes; p++)
                    ff_flif16_pixel_set(ctx, frame, data->permutation[p], r, c, pixel[p]);
            }
        }
    }
}

/*
 * ChannelCompact
 */

static int transform_channelcompact_init(FLIF16TransformContext *ctx,
                                            FLIF16RangesContext *src_ctx)
{
    int p;
    TransformPrivChannelcompact *data = ctx->priv_data;
    if (src_ctx->num_planes > 4)
        return 0;

    for (p = 0; p < 4; p++) {
        data->cpalette_inv_size[p] = 0;
        data->cpalette_size[p]     = 0;
        data->cpalette_inv[p]      = 0;
        data->cpalette[p]          = 0;
    }
    ff_flif16_chancecontext_init(&data->ctx_a);
    return 1;
}

static int transform_channelcompact_read(FLIF16TransformContext *ctx,
                                         FLIF16Context *dec_ctx,
                                         FLIF16RangesContext *src_ctx)
{
    unsigned int nb;
    TransformPrivChannelcompact *data = ctx->priv_data;

    for (; ctx->i < dec_ctx->num_planes; ctx->i++) {
        switch (ctx->segment) {
        case 0:
            RAC_GET(&dec_ctx->rc, &data->ctx_a, 0,
                    ff_flif16_ranges_max(src_ctx, ctx->i) - ff_flif16_ranges_min(src_ctx, ctx->i),
                    &nb, FLIF16_RAC_NZ_INT);
            nb += 1;
            data->min = ff_flif16_ranges_min(src_ctx, ctx->i);
            data->cpalette[ctx->i] = av_malloc_array(nb, sizeof(FLIF16ColorVal));
            if (!data->cpalette[ctx->i])
                return AVERROR(ENOMEM);
            data->cpalette_size[ctx->i] = nb;
            data->remaining = nb-1;
            ctx->segment = 1;

        case 1:
            for (; data->i < data->cpalette_size[ctx->i]; ++data->i) {
                RAC_GET(&dec_ctx->rc, &data->ctx_a, 0,
                        ff_flif16_ranges_max(src_ctx, ctx->i)- data->min - data->remaining,
                        &data->cpalette[ctx->i][data->i],
                        FLIF16_RAC_NZ_INT);
                data->cpalette[ctx->i][data->i] += data->min;
                data->min = data->cpalette[ctx->i][data->i]+1;
                data->remaining--;
            }
            data->i = 0;
            ctx->segment = 0;
        }
    }

    ctx->i = 0;
    ctx->segment = 0;
    return 1;

need_more_data:
    return AVERROR(EAGAIN);
}

static FLIF16RangesContext *transform_channelcompact_meta(FLIF16Context *ctx,
                                                          FLIF16PixelData *frame,
                                                          uint32_t frame_count,
                                                          FLIF16TransformContext *t_ctx,
                                                          FLIF16RangesContext *src_ctx)
{
    int i;
    FLIF16RangesContext *r_ctx;
    RangesPrivChannelcompact *data;
    TransformPrivChannelcompact *trans_data;

    r_ctx = av_mallocz(sizeof(*r_ctx));
    if (!r_ctx)
        return NULL;
    data = av_mallocz(sizeof(*data));
    if (!data) {
        av_freep(&r_ctx);
        return NULL;
    }
    trans_data = t_ctx->priv_data;
    r_ctx->num_planes = src_ctx->num_planes;
    for (i = 0; i < src_ctx->num_planes; i++) {
        data->nb_colors[i] = trans_data->cpalette_size[i] - 1;
    }
    r_ctx->priv_data = data;
    r_ctx->r_no = FLIF16_RANGES_CHANNELCOMPACT;
    ff_static_close(src_ctx);
    av_freep(&src_ctx->priv_data);
    av_freep(&src_ctx);

    return r_ctx;
}

static void transform_channelcompact_reverse(FLIF16Context *ctx,
                                            FLIF16TransformContext *t_ctx,
                                            FLIF16PixelData *frame,
                                            uint32_t stride_row,
                                            uint32_t stride_col)
{
    int p, P;
    uint32_t r, c;
    FLIF16ColorVal *palette;
    size_t palette_size;
    TransformPrivChannelcompact *data = t_ctx->priv_data;

    for (p = 0; p < ctx->num_planes; p++) {
        palette_size = data->cpalette_size[p];
        palette      = data->cpalette[p];

        for (r = 0; r < ctx->height; r += stride_row) {
            for (c = 0; c < ctx->width; c += stride_col) {
                P = ff_flif16_pixel_get(ctx, frame, p, r, c);
                if (P < 0 || P >= (int) palette_size)
                    P = 0;
                av_assert1(P < (int) palette_size);
                ff_flif16_pixel_set(ctx, frame, p, r, c, palette[P]);
            }
        }
    }
}

static void transform_channelcompact_close(FLIF16TransformContext *ctx)
{
    TransformPrivChannelcompact *data = ctx->priv_data;
    for (unsigned int i = 0; i < 4; i++) {
        av_freep(&data->cpalette[i]);

        if (data->cpalette_inv_size[i])
            av_freep(&data->cpalette_inv[i]);
    }
}

/*
 * Bounds
 */

static int transform_bounds_init(FLIF16TransformContext *ctx,
                                 FLIF16RangesContext *src_ctx)
{
    TransformPrivBounds *data = ctx->priv_data;
    if (src_ctx->num_planes > 4)
        return 0;
    ff_flif16_chancecontext_init(&data->ctx_a);
    data->bounds = av_malloc_array(src_ctx->num_planes, sizeof(*data->bounds));
    if (!data->bounds)
        return AVERROR(ENOMEM);
    return 1;
}

static int transform_bounds_read(FLIF16TransformContext *ctx,
                                 FLIF16Context *dec_ctx,
                                 FLIF16RangesContext *src_ctx)
{
    TransformPrivBounds *data = ctx->priv_data;
    int max;

    for (; ctx->i < dec_ctx->num_planes; ctx->i++) {
        switch (ctx->segment) {
        case 0:
            RAC_GET(&dec_ctx->rc, &data->ctx_a, ff_flif16_ranges_min(src_ctx, ctx->i),
                    ff_flif16_ranges_max(src_ctx, ctx->i), &data->min, FLIF16_RAC_GNZ_INT);
            ctx->segment = 1;

        case 1:
            RAC_GET(&dec_ctx->rc, &data->ctx_a, data->min,
                    ff_flif16_ranges_max(src_ctx, ctx->i), &max, FLIF16_RAC_GNZ_INT);
            if (data->min > max)
                return 0;
            if (data->min < ff_flif16_ranges_min(src_ctx, ctx->i))
                return 0;
            if (max > ff_flif16_ranges_max(src_ctx, ctx->i))
                return 0;
            data->bounds[ctx->i][0] = data->min;
            data->bounds[ctx->i][1] = max;
            ctx->segment = 0;
        }
    }

    ctx->i = 0;
    ctx->segment = 0;
    return 1;

need_more_data:
    return AVERROR(EAGAIN);
}

static FLIF16RangesContext *transform_bounds_meta(FLIF16Context *ctx,
                                                  FLIF16PixelData *frame,
                                                  uint32_t frame_count,
                                                  FLIF16TransformContext *t_ctx,
                                                  FLIF16RangesContext *src_ctx)
{
    FLIF16RangesContext *r_ctx;
    TransformPrivBounds *trans_data = t_ctx->priv_data;
    RangesPrivStatic *data;
    RangesPrivBounds *dataB;

    r_ctx = av_mallocz(sizeof(*r_ctx));
    if (!r_ctx)
        return NULL;
    r_ctx->num_planes = src_ctx->num_planes;

    if (flif16_ranges[src_ctx->r_no]->is_static) {
        r_ctx->r_no = FLIF16_RANGES_STATIC;
        r_ctx->priv_data = av_mallocz(sizeof(*data));
        if (!r_ctx->priv_data) {
            av_free(r_ctx);
            return NULL;
        }
        data = r_ctx->priv_data;
        data->bounds = trans_data->bounds;
        ff_flif16_ranges_close(src_ctx);
    } else {
        r_ctx->r_no = FLIF16_RANGES_BOUNDS;
        r_ctx->priv_data = av_mallocz(sizeof(*dataB));
        if (!r_ctx->priv_data) {
            av_freep(&r_ctx);
            return NULL;
        }
        dataB = r_ctx->priv_data;
        dataB->bounds = trans_data->bounds;
        dataB->r_ctx = src_ctx;
    }
    return r_ctx;
}

/*
 * Palette
 */

#define MAX_PALETTE_SIZE 30000

static int transform_palette_init(FLIF16TransformContext *ctx,
                                  FLIF16RangesContext *src_ctx)
{
    TransformPrivPalette *data = ctx->priv_data;

    if ((src_ctx->num_planes < 3)
              ||
        (ff_flif16_ranges_max(src_ctx, 0) == 0 &&
         ff_flif16_ranges_max(src_ctx, 2) == 0 &&
         src_ctx->num_planes > 3               &&
         ff_flif16_ranges_min(src_ctx, 3) == 1 &&
         ff_flif16_ranges_max(src_ctx, 3) == 1)
              ||
        (ff_flif16_ranges_min(src_ctx, 1) == ff_flif16_ranges_max(src_ctx, 1) &&
         ff_flif16_ranges_min(src_ctx, 2) == ff_flif16_ranges_max(src_ctx, 2)))
        return 0;

    if (src_ctx->num_planes > 3)
        data->has_alpha = 1;
    else
        data->has_alpha = 0;

    ff_flif16_chancecontext_init(&data->ctx);
    ff_flif16_chancecontext_init(&data->ctx_y);
    ff_flif16_chancecontext_init(&data->ctx_i);
    ff_flif16_chancecontext_init(&data->ctx_q);
    data->p = 0;

    return 1;
}

static int transform_palette_read(FLIF16TransformContext *ctx,
                                     FLIF16Context *dec_ctx,
                                     FLIF16RangesContext *src_ctx)
{
    TransformPrivPalette *data = ctx->priv_data;

    switch (ctx->i) {
    case 0:
        RAC_GET(&dec_ctx->rc, &data->ctx, 1, MAX_PALETTE_SIZE,
                &data->size, FLIF16_RAC_GNZ_INT);
        data->palette = av_malloc_array(data->size, sizeof(*data->palette));
        if (!data->palette)
            return AVERROR(ENOMEM);
        ctx->i = 1;

    case 1:
        RAC_GET(&dec_ctx->rc, &data->ctx, 0, 1,
                &data->sorted, FLIF16_RAC_GNZ_INT);
        if (data->sorted) {
            ctx->i = 2;
            for (int i = 0; i < 3; i++) {
                data->min[i] = ff_flif16_ranges_min(src_ctx, i);
                data->max[i] = ff_flif16_ranges_max(src_ctx, i);
                data->palette[0][i] = -1;
            }
            data->prev = data->palette[0];
        } else {
            ctx->i = 5;
        }
    }

    for (; data->p < data->size; data->p++) {
        switch (ctx->i) {
        case 2:
            RAC_GET(&dec_ctx->rc, &data->ctx_y, data->min[0], data->max[0],
                    &data->y, FLIF16_RAC_GNZ_INT);
            data->pp[0] = data->y;
            ff_flif16_ranges_minmax(src_ctx, 1, data->pp, &data->min[1], &data->max[1]);
            ctx->i = 3;

        case 3:
            RAC_GET(&dec_ctx->rc, &data->ctx_i,
                    data->prev[0] == data->y ? data->prev[1] : data->min[1],
                    data->max[1],
                    &data->i, FLIF16_RAC_GNZ_INT);
            data->pp[1] = data->i;
            ff_flif16_ranges_minmax(src_ctx, 2, data->pp, &data->min[2], &data->max[2]);
            ctx->i = 4;

        case 4:
            RAC_GET(&dec_ctx->rc, &data->ctx_q, data->min[2], data->max[2],
                    &data->q, FLIF16_RAC_GNZ_INT);
            data->palette[data->p][0] = data->y;
            data->palette[data->p][1] = data->i;
            data->palette[data->p][2] = data->q;
            data->min[0] = data->y;
            data->prev = data->palette[data->p];
            ctx->i = 2;
        }
    }

    for (; data->p < data->size; data->p++) {
        switch (ctx->i) {
        case 5:
            ff_flif16_ranges_minmax(src_ctx, 0, data->pp, &data->min[0], &data->max[0]);
            RAC_GET(&dec_ctx->rc, &data->ctx_y, data->min[0], data->max[0],
                    &data->y, FLIF16_RAC_GNZ_INT);
            data->pp[0] = data->y;
            ctx->i = 6;

        case 6:
            ff_flif16_ranges_minmax(src_ctx, 1, data->pp, &data->min[0], &data->max[0]);
            RAC_GET(&dec_ctx->rc, &data->ctx_i, data->min[0], data->max[0],
                    &data->i, FLIF16_RAC_GNZ_INT);
            data->pp[1] = data->i;
            ctx->i = 7;

        case 7:
            ff_flif16_ranges_minmax(src_ctx, 2, data->pp, &data->min[0], &data->max[0]);
            RAC_GET(&dec_ctx->rc, &data->ctx_q, data->min[0], data->max[0],
                    &data->q, FLIF16_RAC_GNZ_INT);
            data->palette[data->p][0] = data->y;
            data->palette[data->p][1] = data->i;
            data->palette[data->p][2] = data->q;
            ctx->i = 5;
        }
    }

    ctx->i = 0;
    data->p = 0;
    return 1;

need_more_data:
    return AVERROR(EAGAIN);
}

static FLIF16RangesContext *transform_palette_meta(FLIF16Context *ctx,
                                                   FLIF16PixelData *frame,
                                                   uint32_t frame_count,
                                                   FLIF16TransformContext *t_ctx,
                                                   FLIF16RangesContext *src_ctx)
{
    FLIF16RangesContext *r_ctx;
    TransformPrivPalette *trans_data;
    RangesPrivPalette *data;

    r_ctx = av_mallocz(sizeof(*r_ctx));
    if (!r_ctx)
        return NULL;
    trans_data = t_ctx->priv_data;
    data = av_mallocz(sizeof(*data));

    if (!data) {
        av_freep(&r_ctx);
        return NULL;
    }

    data->r_ctx = src_ctx;
    data->nb_colors = trans_data->size;
    r_ctx->r_no = FLIF16_RANGES_PALETTE;
    r_ctx->num_planes = src_ctx->num_planes;
    r_ctx->priv_data = data;
    return r_ctx;
}

static void transform_palette_reverse(FLIF16Context *ctx,
                                     FLIF16TransformContext *t_ctx,
                                     FLIF16PixelData *frame,
                                     uint32_t stride_row,
                                     uint32_t stride_col)
{
    int r, c;
    int p;
    FLIF16ColorVal (*v)[3];
    TransformPrivPalette *data = t_ctx->priv_data;
    for (r = 0; r < ctx->height; r += stride_row) {
        for (c = 0; c < ctx->width; c += stride_col) {
            p = ff_flif16_pixel_get(ctx, frame, 1, r, c);
            if (p < 0 || p >= data->size)
                p = 0;
            av_assert1(p < data->size);
            av_assert1(p >= 0);

            v = &data->palette[p];
            for (unsigned int i = 0; i < 3; i++)
                ff_flif16_pixel_set(ctx, frame, i, r, c, (*v)[i]);
        }
    }
}

static void transform_palette_close(FLIF16TransformContext *ctx)
{
    TransformPrivPalette *data = ctx->priv_data;
    av_freep(&data->palette);
}

/*
 * Palette Alpha
 */

static int transform_palettealpha_init(FLIF16TransformContext *ctx,
                                          FLIF16RangesContext *src_ctx)
{
    TransformPrivPalettealpha *data = ctx->priv_data;
    if (src_ctx->num_planes < 4 ||
        ff_flif16_ranges_min(src_ctx, 3) == ff_flif16_ranges_max(src_ctx, 3))
        return 0;

    data->already_has_palette = 0;
    ff_flif16_chancecontext_init(&data->ctx);
    ff_flif16_chancecontext_init(&data->ctx_y);
    ff_flif16_chancecontext_init(&data->ctx_i);
    ff_flif16_chancecontext_init(&data->ctx_q);
    ff_flif16_chancecontext_init(&data->ctx_a);
    data->p = 0;

    return 1;
}

static int transform_palettealpha_read(FLIF16TransformContext *ctx,
                                          FLIF16Context *dec_ctx,
                                          FLIF16RangesContext *src_ctx)
{
    TransformPrivPalettealpha *data = ctx->priv_data;

    switch (ctx->i) {
    case 0:
        RAC_GET(&dec_ctx->rc, &data->ctx, 1, MAX_PALETTE_SIZE,
                &data->size, FLIF16_RAC_GNZ_INT);
        data->palette = av_malloc_array(data->size, sizeof(*data->palette));
        if (!data->palette)
            return 0;
        ctx->i++;

    case 1:
        RAC_GET(&dec_ctx->rc, &data->ctx, 0, 1,
                &data->sorted, FLIF16_RAC_GNZ_INT);
        if (data->sorted) {
            ctx->i = 2;
            data->min[0] = ff_flif16_ranges_min(src_ctx, 3);
            data->max[0] = ff_flif16_ranges_max(src_ctx, 3);
            for (int i = 1; i < 4; i++) {
                data->min[i] = ff_flif16_ranges_min(src_ctx, i-1);
                data->max[i] = ff_flif16_ranges_max(src_ctx, i-1);
                data->palette[0][i] = -1;
            }
            data->prev = data->palette[0];
        } else {
            ctx->i = 6;
        }
    }

    for (; data->p < data->size && ctx->i < 6; data->p++) {
        switch (ctx->i) {
        case 2:
            RAC_GET(&dec_ctx->rc, &data->ctx_a, data->min[0], data->max[0],
                    &data->a, FLIF16_RAC_GNZ_INT);
            if (data->alpha_zero_special && data->a == 0) {
                for (int i = 0; i < 4; i++)
                    data->palette[data->p][i] = 0;
                break;
            }
            ctx->i = 3;

        case 3:
            RAC_GET(&dec_ctx->rc, &data->ctx_y,
                    data->prev[0] == data->a ? data->prev[1] : data->min[1],
                    data->max[1],
                    &data->y, FLIF16_RAC_GNZ_INT);
            data->pp[0] = data->y;
            ff_flif16_ranges_minmax(src_ctx, 1, data->pp, &data->min[2], &data->max[2]);
            ctx->i = 4;

        case 4:
            RAC_GET(&dec_ctx->rc, &data->ctx_i,
                    data->min[2], data->max[2],
                    &data->i, FLIF16_RAC_GNZ_INT);
            data->pp[1] = data->i;
            ff_flif16_ranges_minmax(src_ctx, 2, data->pp, &data->min[3], &data->max[3]);
            ctx->i = 5;

        case 5:
            RAC_GET(&dec_ctx->rc, &data->ctx_q, data->min[3], data->max[3],
                    &data->q, FLIF16_RAC_GNZ_INT);
            data->palette[data->p][0] = data->a;
            data->palette[data->p][1] = data->y;
            data->palette[data->p][2] = data->i;
            data->palette[data->p][3] = data->q;
            data->min[0] = data->a;
            data->prev = data->palette[data->p];
            ctx->i = 2;
        }
    }

    for (; data->p < data->size && ctx->i >=6; data->p++) {
        switch (ctx->i) {
        case 6:
            RAC_GET(&dec_ctx->rc, &data->ctx_a,
            ff_flif16_ranges_min(src_ctx, 3), ff_flif16_ranges_max(src_ctx, 3),
            &data->a, FLIF16_RAC_GNZ_INT);
            if (data->alpha_zero_special && data->a == 0) {
                for (int i = 0; i < 4; i++)
                    data->palette[data->p][i] = 0;
                data->p++;
            }
            ctx->i = 7;

        case 7:
            ff_flif16_ranges_minmax(src_ctx, 0, data->pp, &data->min[0], &data->max[0]);
            RAC_GET(&dec_ctx->rc, &data->ctx_y, data->min[0], data->max[0],
                    &data->y, FLIF16_RAC_GNZ_INT);
            data->pp[0] = data->y;
            ctx->i = 8;

        case 8:
            ff_flif16_ranges_minmax(src_ctx, 1, data->pp, &data->min[0], &data->max[0]);
            RAC_GET(&dec_ctx->rc, &data->ctx_i, data->min[0], data->max[0],
                    &data->i, FLIF16_RAC_GNZ_INT);
            data->pp[1] = data->i;
            ctx->i = 9;

        case 9:
            ff_flif16_ranges_minmax(src_ctx, 2, data->pp, &data->min[0], &data->max[0]);
            RAC_GET(&dec_ctx->rc, &data->ctx_q, data->min[0], data->max[0],
                    &data->q, FLIF16_RAC_GNZ_INT);
            data->palette[data->p][0] = data->a;
            data->palette[data->p][1] = data->y;
            data->palette[data->p][2] = data->i;
            data->palette[data->p][3] = data->q;
            ctx->i = 6;
        }
    }

    data->p = 0;
    ctx->i = 0;
    return 1;

need_more_data:
    return AVERROR(EAGAIN);
}

static void transform_palettealpha_configure(FLIF16TransformContext *ctx,
                                             const int setting)
{
    TransformPrivPalettealpha *data = ctx->priv_data;
    data->alpha_zero_special = setting;
    if (setting > 0) {
        data->ordered_palette = 1;
        data->max_palette_size = setting;
    } else {
        data->ordered_palette = 0;
        data->max_palette_size = -setting;
    }
}

static FLIF16RangesContext *transform_palettealpha_meta(FLIF16Context *ctx,
                                                        FLIF16PixelData *frame,
                                                        uint32_t frame_count,
                                                        FLIF16TransformContext *t_ctx,
                                                        FLIF16RangesContext *src_ctx)
{
    FLIF16RangesContext *r_ctx;
    TransformPrivPalettealpha *data;
    RangesPrivPalette *priv_data;
    r_ctx = av_mallocz(sizeof(*r_ctx));
    if (!r_ctx)
        return NULL;
    data = t_ctx->priv_data;

    priv_data = av_mallocz(sizeof(*priv_data));
    if (!priv_data) {
        av_freep(&r_ctx);
        return NULL;
    }
    r_ctx->r_no = FLIF16_RANGES_PALETTEALPHA;
    r_ctx->num_planes = src_ctx->num_planes;
    priv_data->nb_colors = data->size;
    priv_data->r_ctx = src_ctx;
    r_ctx->priv_data = priv_data;

    return r_ctx;
}

static void transform_palettealpha_reverse(FLIF16Context *ctx,
                                          FLIF16TransformContext *t_ctx,
                                          FLIF16PixelData *frame,
                                          uint32_t stride_row,
                                          uint32_t stride_col)
{
    int r, c;
    int p;
    TransformPrivPalettealpha *data = t_ctx->priv_data;
    for (r = 0; r < ctx->height; r += stride_row) {
        for (c = 0; c < ctx->width; c += stride_col) {
            p = ff_flif16_pixel_get(ctx, frame, 1, r, c);
            av_assert1(p < data->size);
            ff_flif16_pixel_set(ctx, frame, 0, r, c, data->palette[p][1]);
            ff_flif16_pixel_set(ctx, frame, 1, r, c, data->palette[p][2]);
            ff_flif16_pixel_set(ctx, frame, 2, r, c, data->palette[p][3]);
            ff_flif16_pixel_set(ctx, frame, 3, r, c, data->palette[p][0]);
        }
    }
}

static void transform_palettealpha_close(FLIF16TransformContext *ctx)
{
    TransformPrivPalettealpha *data = ctx->priv_data;
    av_freep(&data->palette);
}

/*
 * ColorBuckets
 */

static int ff_remove_color(ColorBucket *cb, const FLIF16ColorVal c)
{
    if (cb->discrete) {
        unsigned int pos = 0;
        ColorValCB_list *temp = cb->values;
        ColorValCB_list *prev = 0;
        for (; pos < cb->values_size; pos++, temp = temp->next) {
            if (c == temp->data) {
                if (prev && temp != cb->values_last) {
                    prev->next = temp->next;
                    av_freep(&temp);
                } else if (temp == cb->values_last) {
                    cb->values_last = prev;
                    cb->values_last->next = NULL;
                    av_freep(&temp);
                } else if (!prev) {
                    cb->values = temp->next;
                    av_freep(&temp);
                }
                cb->values_size--;
                break;
            }
            prev = temp;
        }
        if (cb->values_size == 0) {
            cb->min = 10000;
            cb->max = -10000;
            return 1;
        }
        av_assert1(cb->values_size > 0);
        if (c == cb->min)
            cb->min = cb->values->data;
        if (c == cb->max)
            cb->max = cb->values_last->data;
    } else {
        if (c == cb->min)
            cb->min++;
        if (c == cb->max)
            cb->max--;
        if (c > cb->max)
            return 1;
        if (c < cb->min)
            return 1;
        cb->discrete = 1;
        av_freep(&cb->values);
        cb->values_size = 0;
        for (FLIF16ColorVal x = cb->min; x <= cb->max; x++) {
            if (x != c) {
                if (cb->values_size == 0) {
                    cb->values = av_mallocz(sizeof(*cb->values));
                    if (!cb->values)
                        return AVERROR(ENOMEM);
                    cb->values_last = cb->values;
                } else {
                    cb->values_last->next = av_mallocz(sizeof(*cb->values_last->next));
                    if (!cb->values_last->next)
                        return AVERROR(ENOMEM);
                    cb->values_last = cb->values_last->next;
                }
                cb->values_last->data = x;
                cb->values_size++;
            }
        }
        cb->values_last->next = NULL;
    }
    return 1;
}

static FLIF16ColorVal ff_snap_color_slow(ColorBucket *cb, const FLIF16ColorVal c)
{
    FLIF16ColorVal diff;
    FLIF16ColorVal d;
    if (c <= cb->min)
        return cb->min;
    if (c >= cb->max)
        return cb->max;
    if (cb->discrete) {
        FLIF16ColorVal mindiff = abs(c - cb->min);
        ColorValCB_list *best = cb->values;
        ColorValCB_list *temp = cb->values->next;
        for (unsigned int i = 1; i < cb->values_size; i++, temp = temp->next) {
            if (c == temp->data)
                return c;
            diff = abs(c - temp->data);
            if (diff < mindiff) {
                best = temp;
                mindiff = diff;
            }
            if (temp->data > c)
                break;
        }
        d = best->data;
        return d;
    }
    return c;
}

static void ff_prepare_snapvalues(ColorBucket *cb)
{
    int i = 0;
    if (cb->discrete) {
        if (cb->max > cb->min) {
            cb->snapvalues = av_malloc_array((cb->max - cb->min), sizeof(*cb->snapvalues));
            cb->snapvalues_size = cb->max - cb->min;
        }
        if (cb->max - cb->min > 0)
            av_assert0(cb->snapvalues != NULL);
        for (FLIF16ColorVal c = cb->min; c < cb->max; c++) {
            cb->snapvalues[i] = ff_snap_color_slow(cb, c);
            i++;
        }
    }
}

static uint8_t ff_colorbuckets_exists2(ColorBuckets *cb, const int p,
                                       FLIF16ColorVal *pp)
{
    FLIF16ColorVal rmin, rmax, v;
    ColorBucket *b;
    if (p > FLIF16_PLANE_Y &&
       (pp[0] < cb->min0 || pp[0] > ff_flif16_ranges_max(cb->ranges, 0))) {
        return 0;
    }
    if (p > FLIF16_PLANE_CO &&
       (pp[1] < cb->min1 || pp[1] > ff_flif16_ranges_max(cb->ranges, 1))) {
        return 0;
    }

    v = pp[p];
    ff_flif16_ranges_snap(cb->ranges, p, pp, &rmin, &rmax, &v);
    if (v != pp[p])
        return 0;

    b = ff_bucket_buckets(cb, p, pp);
    if (ff_snap_color_slow(b, pp[p]) != pp[p])
        return 0;

    return 1;
}

static uint8_t ff_colorbuckets_exists(ColorBuckets *cb, const int p,
                                      FLIF16ColorVal *lower, FLIF16ColorVal *upper)
{
    FLIF16ColorVal pixel[2];
    pixel[0] = lower[0];
    pixel[1] = lower[1];
    if (p == FLIF16_PLANE_Y) {
        for (pixel[0] = lower[0]; pixel[0] <= upper[0]; pixel[0]++) {
            if (ff_colorbuckets_exists2(cb, p, pixel))
                return 1;
        }
    }
    if (p == FLIF16_PLANE_CO) {
        for (pixel[0] = lower[0]; pixel[0] <= upper[0]; pixel[0]++) {
            for (pixel[1] = lower[1]; pixel[1] <= upper[1]; pixel[1]++) {
                if (ff_colorbuckets_exists2(cb, p, pixel))
                    return 1;
            }
        }
    }

    return 0;
}

static int transform_colorbuckets_init(FLIF16TransformContext *ctx,
                                          FLIF16RangesContext *src_ctx)
{
    TransformPrivColorbuckets *data = ctx->priv_data;
    int length, temp;
    ColorBuckets *cb;
    data->cb = NULL;
    data->really_used = 0;
    if ((src_ctx->num_planes < 3)
            ||
       (ff_flif16_ranges_min(src_ctx, 0) == 0 &&
        ff_flif16_ranges_max(src_ctx, 0) == 0 &&
        ff_flif16_ranges_min(src_ctx, 2) == 0 &&
        ff_flif16_ranges_max(src_ctx, 2) == 0)
            ||
       (ff_flif16_ranges_min(src_ctx, 0) == ff_flif16_ranges_max(src_ctx, 0) &&
        ff_flif16_ranges_min(src_ctx, 1) == ff_flif16_ranges_max(src_ctx, 1) &&
        ff_flif16_ranges_min(src_ctx, 2) == ff_flif16_ranges_max(src_ctx, 2))
            ||
       (ff_flif16_ranges_max(src_ctx, 0) - ff_flif16_ranges_min(src_ctx, 0) > 1023 ||
        ff_flif16_ranges_max(src_ctx, 1) - ff_flif16_ranges_min(src_ctx, 1) > 1023 ||
        ff_flif16_ranges_max(src_ctx, 2) - ff_flif16_ranges_min(src_ctx, 2) > 1023)
            ||
       (ff_flif16_ranges_min(src_ctx, 1) == ff_flif16_ranges_max(src_ctx, 1)))
        return 0;

    cb = av_mallocz(sizeof(*cb));
    if (!cb)
        return AVERROR(ENOMEM);

    ff_init_bucket_default(&cb->bucket0);
    cb->min0 = ff_flif16_ranges_min(src_ctx, 0);
    cb->min1 = ff_flif16_ranges_min(src_ctx, 1);

    length = ((ff_flif16_ranges_max(src_ctx, 0) - cb->min0)/1 + 1);
    temp = ((ff_flif16_ranges_max(src_ctx, 1) - cb->min1)/4 + 1);

    cb->bucket1 = av_malloc_array(((ff_flif16_ranges_max(src_ctx, 0) - cb->min0)/1 + 1),
                                    sizeof(*cb->bucket1));
    if (!cb->bucket1) {
        av_freep(&cb);
        return AVERROR(ENOMEM);
    }
    cb->bucket1_size = ((ff_flif16_ranges_max(src_ctx, 0)
                                   - cb->min0)/1 + 1);
    for (unsigned int i = 0; i < cb->bucket1_size; i++)
        ff_init_bucket_default(&cb->bucket1[i]);
    cb->bucket2 = av_malloc_array(length, sizeof(*cb->bucket2));
    if (!cb->bucket2) {
        av_freep(&cb->bucket1);
        av_freep(&cb);
        return AVERROR(ENOMEM);
    }
    cb->bucket2_size = length;
    for (unsigned int i = 0; i < length; i++) {
        cb->bucket2_list_size = temp;
        cb->bucket2[i] = av_malloc_array(temp, sizeof(*cb->bucket2[i]));

        if (!cb->bucket2[i]) {
            av_freep(&cb->bucket1);
            av_freep(&cb->bucket2);
            av_freep(&cb);
            return AVERROR(ENOMEM);
        }

        for (unsigned int j = 0; j < temp; j++)
            ff_init_bucket_default(&cb->bucket2[i][j]);
    }
    ff_init_bucket_default(&cb->bucket3);
    for (uint8_t i = 0; i < 6; i++)
        ff_flif16_chancecontext_init(&data->ctx[i]);

    cb->ranges = src_ctx;
    data->cb = cb;
    data->i = 0;

    return 1;
}

static FLIF16RangesContext *transform_colorbuckets_meta(FLIF16Context *ctx,
                                                        FLIF16PixelData *frame,
                                                        uint32_t frame_count,
                                                        FLIF16TransformContext *t_ctx,
                                                        FLIF16RangesContext *src_ctx)
{
    FLIF16RangesContext *r_ctx;
    TransformPrivColorbuckets *trans_data = t_ctx->priv_data;
    RangesPrivColorbuckets *data;
    ColorBuckets *cb = trans_data->cb;
    FLIF16ColorVal pixel_l[2], pixel_u[2];

    r_ctx = av_mallocz(sizeof(*r_ctx));
    if (!r_ctx)
        return NULL;
    data = av_mallocz(sizeof(*data));
    if (!data) {
        av_freep(&r_ctx);
        return NULL;
    }
    if (ff_flif16_ranges_min(src_ctx, 2) < ff_flif16_ranges_max(src_ctx, 2)) {
        pixel_l[0] = cb->min0;
        pixel_u[0] = cb->min0 + 1 - 1;
        pixel_l[1] = cb->min1;
        pixel_u[1] = cb->min1 + 4 - 1;
        for (int i = 0; i < cb->bucket2_size; i++) {
            pixel_l[1] = cb->min1;
            pixel_u[1] = cb->min1 + 4 - 1;
            for (int j = 0; j < cb->bucket2_list_size; j++) {
                if (cb->bucket2[i][j].min > cb->bucket2[i][j].max) {
                    for (FLIF16ColorVal c = pixel_l[1]; c <= pixel_u[1]; c++) {
                        if (!ff_remove_color(ff_bucket_buckets2(cb, 1, pixel_l), c))
                            return NULL;
                        if (!ff_remove_color(ff_bucket_buckets2(cb, 1, pixel_u), c))
                            return NULL;
                    }
                }
                pixel_l[1] += 4;
                pixel_u[1] += 4;
            }
            pixel_l[0] += 1;
            pixel_u[0] += 1;
        }
    }
    ff_prepare_snapvalues(&cb->bucket0);
    ff_prepare_snapvalues(&cb->bucket3);
    for (unsigned int i = 0; i < cb->bucket1_size; i++)
        ff_prepare_snapvalues(&cb->bucket1[i]);
    for (unsigned int i = 0; i < cb->bucket2_size; i++) {
        for (unsigned int j = 0; j < cb->bucket2_list_size; j++)
            ff_prepare_snapvalues(&cb->bucket2[i][j]);
    }

    trans_data->really_used = 1;

    data->r_ctx = src_ctx;
    data->buckets = trans_data->cb;

    r_ctx->r_no = FLIF16_RANGES_COLORBUCKETS;
    r_ctx->priv_data = data;
    r_ctx->num_planes = src_ctx->num_planes;

    return r_ctx;
}

static void transform_colorbuckets_minmax(FLIF16RangesContext *src_ctx, int p,
                                          FLIF16ColorVal *lower,
                                          FLIF16ColorVal *upper,
                                          FLIF16ColorVal *smin,
                                          FLIF16ColorVal *smax)
{
    FLIF16ColorVal rmin, rmax;
    FLIF16ColorVal pixel[2];
    pixel[0] = lower[0];
    pixel[1] = lower[1];
    *smin = 10000;
    *smax = -10000;
    if (p == FLIF16_PLANE_Y) {
        ff_flif16_ranges_minmax(src_ctx, p,pixel,smin,smax);
    } else if (p == FLIF16_PLANE_CO) {
        for (pixel[0] = lower[0]; pixel[0] <= upper[0]; pixel[0]++) {
            ff_flif16_ranges_minmax(src_ctx, p, pixel, &rmin, &rmax);
            *smin = FFMIN(*smin, rmin);
            *smax = FFMAX(*smax, rmax);
        }
    } else if (p == FLIF16_PLANE_CG) {
        for (pixel[0] = lower[0]; pixel[0] <= upper[0]; pixel[0]++) {
            for (pixel[1] = lower[1]; pixel[1] <= upper[1]; pixel[1]++) {
                ff_flif16_ranges_minmax(src_ctx, p, pixel, &rmin, &rmax);
                *smin = FFMIN(*smin, rmin);
                *smax = FFMAX(*smax, rmax);
            }
        }
    } else if (p == FLIF16_PLANE_ALPHA) {
        ff_flif16_ranges_minmax(src_ctx, p, pixel, smin, smax);
    }
}

static const unsigned int max_per_colorbucket[] = {255, 510, 5, 255};

static int ff_load_bucket(FLIF16RangeCoder *rc, FLIF16ChanceContext *chancectx,
                          ColorBucket *b, ColorBuckets *cb,
                          FLIF16RangesContext *src_ctx, int plane,
                          FLIF16ColorVal *pixel_l, FLIF16ColorVal *pixel_u)
{
    int temp;
    int exists;

    switch (cb->i) {
    case 0:
        if (plane < FLIF16_PLANE_ALPHA)
        for (int p = 0; p < plane; p++) {
            if (!ff_colorbuckets_exists(cb, p, pixel_l, pixel_u)) {
                return 1;
            }
        }
        cb->smin = 0;
        cb->smax = 0;
        cb->i = 1;

    case 1:
        transform_colorbuckets_minmax(src_ctx, plane,
                                      pixel_l, pixel_u,
                                      &cb->smin, &cb->smax);
        RAC_GET(rc, &chancectx[0], 0, 1, &exists, FLIF16_RAC_GNZ_INT);
        if (exists == 0) {
            cb->i = 0;
            return 1; // Empty bucket
        }
        if (cb->smin == cb->smax) {
            b->min = cb->smin;
            b->max = cb->smin;
            b->discrete = 0;
            cb->i = 0;
            return 1;
        }
        cb->i = 2;

    case 2:
        RAC_GET(rc, &chancectx[1], cb->smin, cb->smax, &b->min, FLIF16_RAC_GNZ_INT);
        cb->i = 3;

    case 3:
        RAC_GET(rc, &chancectx[2], b->min, cb->smax, &b->max, FLIF16_RAC_GNZ_INT);
        if (b->min == b->max) {
            b->discrete = 0;
            cb->i = 0;
            return 1;
        }
        if (b->min + 1 == b->max) {
            b->discrete = 0;
            cb->i = 0;
            return 1;
        }
        cb->i = 4;

    case 4:
        RAC_GET(rc, &chancectx[3], 0, 1, &b->discrete, FLIF16_RAC_GNZ_INT);
        cb->i = 5;
    }

    if (b->discrete) {
        switch (cb->i) {
        case 5:
            RAC_GET(rc, &chancectx[4], 2,
                    FFMIN(max_per_colorbucket[plane], b->max - b->min),
                    &cb->nb, FLIF16_RAC_GNZ_INT);
            b->values = 0;
            b->values = av_mallocz(sizeof(*b->values));
            if (!b->values)
                return AVERROR(ENOMEM);
            b->values_last = b->values;
            b->values->data = b->min;
            b->values_size++;

            cb->v = b->min;
            cb->i2 = 1;
            cb->i = 6;

        case 6:
            for (; cb->i2 < cb->nb - 1; cb->i2++) {
                RAC_GET(rc, &chancectx[5], cb->v + 1,
                        b->max + 1 - cb->nb + cb->i2, &temp,
                        FLIF16_RAC_GNZ_INT);
                b->values_last->next = av_mallocz(sizeof(*b->values_last->next));
                if (!b->values_last->next)
                    return AVERROR(ENOMEM);
                b->values_last = b->values_last->next;
                b->values_last->data = temp;
                b->values_size++;
                cb->v = temp;
            }
            b->values_last->next = NULL;
            b->values_size = cb->nb - 1;

            if (b->min < b->max) {
                b->values_last->next = av_mallocz(sizeof(*b->values_last->next));
                if (!b->values_last->next)
                    return AVERROR(ENOMEM);
                b->values_last = b->values_last->next;
                b->values_last->data = b->max;
                b->values_last->next = NULL;
                b->values_size++;
            }
        }
    }

    cb->i = 0;
    cb->i2 = 0;
    cb->nb = 0;
    return 1;

need_more_data:
    return AVERROR(EAGAIN);
}

static int transform_colorbuckets_read(FLIF16TransformContext *ctx,
                                       FLIF16Context *dec_ctx,
                                       FLIF16RangesContext *src_ctx)
{
    TransformPrivColorbuckets *data = ctx->priv_data;
    ColorBuckets *cb = data->cb;
    int8_t ret;

    switch (data->i) {
    case 0:
        ret = ff_load_bucket(&dec_ctx->rc, data->ctx, &cb->bucket0, cb,
                             src_ctx, 0, data->pixel_l, data->pixel_u);
        if (ret <= 0)
            return AVERROR(EAGAIN);
        data->pixel_l[0] = cb->min0;
        data->pixel_u[0] = cb->min0;
        data->i = 1;

    case 1:
        for (; data->j < cb->bucket1_size; data->j++) {
            ret = ff_load_bucket(&dec_ctx->rc, data->ctx,
                                 &cb->bucket1[data->j], cb,
                                 src_ctx, 1, data->pixel_l, data->pixel_u);
            if (ret <= 0)
                return AVERROR(EAGAIN);
            data->pixel_l[0] += 1;
            data->pixel_u[0] += 1;
        }
        data->j = 0;

        if (ff_flif16_ranges_min(src_ctx, 2) < ff_flif16_ranges_max(src_ctx, 2)) {
            data->pixel_l[0] = cb->min0;
            data->pixel_u[0] = cb->min0 + 1 - 1;
            data->pixel_l[1] = cb->min1;
            data->pixel_u[1] = cb->min1 + 4 - 1;
            data->i = 2;
        } else
            data->i = 3;
    }

    switch (data->i) {
        for (; data->j < cb->bucket2_size; data->j++) {
            data->pixel_l[1] = cb->min1;
            data->pixel_u[1] = cb->min1 + 4 - 1;
    case 2:
            for (; data->k < cb->bucket2_list_size; data->k++) {
                ret = ff_load_bucket(&dec_ctx->rc, data->ctx,
                                     &cb->bucket2[data->j][data->k], cb,
                                     src_ctx, 2, data->pixel_l, data->pixel_u);
                if (ret <= 0)
                    return AVERROR(EAGAIN);
                data->pixel_l[1] += 4;
                data->pixel_u[1] += 4;
            }
            data->k = 0;
            data->pixel_l[0] += 1;
            data->pixel_u[0] += 1;
        }
        data->j = 0;
        data->i = 3;

    case 3:
        if (src_ctx->num_planes > 3) {
            ret = ff_load_bucket(&dec_ctx->rc, data->ctx, &cb->bucket3, cb,
                                 src_ctx, 3, data->pixel_l, data->pixel_u);
            if (ret <= 0)
                return AVERROR(EAGAIN);
        }

    }

    data->i = 0;
    data->j = 0;
    data->k = 0;
    return 1;
}

static void transform_colorbuckets_close(FLIF16TransformContext *ctx)
{
    TransformPrivColorbuckets *data = ctx->priv_data;
    ff_priv_colorbuckets_close(data->cb);
    av_freep(&data->cb);
}

static int transform_framedup_init(FLIF16TransformContext *ctx,
                                   FLIF16RangesContext *src_ctx)
{
    TransformPrivFramedup *data = ctx->priv_data;
    ff_flif16_chancecontext_init(&data->chancectx);
    data->i = 0;

    return 1;
}

static void transform_framedup_configure(FLIF16TransformContext *ctx,
                                         const int setting)
{
    TransformPrivFramedup *data = ctx->priv_data;
    data->nb = setting;
}

static int transform_framedup_read(FLIF16TransformContext  *ctx,
                                   FLIF16Context *dec_ctx,
                                   FLIF16RangesContext *src_ctx)
{
    TransformPrivFramedup *data = ctx->priv_data;

    switch (ctx->i) {
    case 0:
        data->seen_before = av_malloc_array(data->nb, sizeof(*data->seen_before));
        if (!data->seen_before)
            return 0;
        data->seen_before[0] = -1;
        ctx->i = 1;
        data->i = 1;

    case 1:
        for (; data->i < data->nb; data->i++) {
            RAC_GET(&dec_ctx->rc, &data->chancectx, -1, data->i - 1,
                    &data->seen_before[data->i], FLIF16_RAC_NZ_INT);
        }
        data->i = 0;
        return 1;
    }

    ctx->i = 0;
    return 1;

need_more_data:
    return AVERROR(EAGAIN);
}

static FLIF16RangesContext *transform_framedup_meta(FLIF16Context *ctx,
                                                    FLIF16PixelData *frame,
                                                    uint32_t frame_count,
                                                    FLIF16TransformContext *t_ctx,
                                                    FLIF16RangesContext *src_ctx)
{
    TransformPrivFramedup *data = t_ctx->priv_data;
    for (unsigned int fr = 0; fr < frame_count; fr++) {
        frame[fr].seen_before = data->seen_before[fr];
    }

    return src_ctx;
}

static void transform_framedup_close(FLIF16TransformContext *ctx)
{
    TransformPrivFramedup *data = ctx->priv_data;
    av_freep(&data->seen_before);
}

static int transform_frameshape_init(FLIF16TransformContext *ctx,
                                     FLIF16RangesContext *src_ctx)
{
    TransformPrivFrameshape *data = ctx->priv_data;
    ff_flif16_chancecontext_init(&data->chancectx);
    data->i = 0;

    return 1;
}

static void transform_frameshape_configure(FLIF16TransformContext *ctx,
                                           const int setting)
{
    TransformPrivFrameshape *data = ctx->priv_data;
    if (data->nb == 0) {
        data->nb = setting;
    } else
        data->cols = setting;
}

static int transform_frameshape_read(FLIF16TransformContext  *ctx,
                                        FLIF16Context *dec_ctx,
                                        FLIF16RangesContext *src_ctx)
{
    TransformPrivFrameshape *data = ctx->priv_data;
    int temp;

    switch (ctx->i) {
    case 0:
        data->b = av_malloc_array(data->nb, sizeof(*data->b));
        if (!data->b)
            return AVERROR(ENOMEM);
        data->e = av_malloc_array(data->nb, sizeof(*data->e));
        if (!data->e) {
            av_freep(&data->b);
            return AVERROR(ENOMEM);
        }
        ctx->i = 1;

    case 1:
        for (; data->i < data->nb; data->i++) {
            RAC_GET(&dec_ctx->rc, &data->chancectx, 0, data->cols,
                    &data->b[data->i], FLIF16_RAC_NZ_INT);
        }
        ctx->i = 2;
        data->i = 0;

    case 2:
        for (; data->i < data->nb; data->i++) {
            temp = ff_flif16_rac_process(&dec_ctx->rc, &data->chancectx, 0,
                                         data->cols - data->b[data->i],
                                         &data->e[data->i], FLIF16_RAC_NZ_INT);
            if (temp == 0)
                return AVERROR(EAGAIN);
            data->e[data->i] = data->cols - data->e[data->i];

            if (data->e[data->i] > data->cols       ||
                data->e[data->i] < data->b[data->i] ||
                data->e[data->i] <= 0)
                    return 0;
        }
        data->i = 0;
    }

    ctx->i = 0;
    return 1;

need_more_data:
    return AVERROR(EAGAIN);
}

static FLIF16RangesContext *transform_frameshape_meta(FLIF16Context *ctx,
                                                      FLIF16PixelData *frame,
                                                      uint32_t frame_count,
                                                      FLIF16TransformContext *t_ctx,
                                                      FLIF16RangesContext *src_ctx)
{
    TransformPrivFrameshape *data = t_ctx->priv_data;
    uint32_t pos = 0;

    for (unsigned int fr = 1; fr < frame_count; fr++) {
        if (frame[fr].seen_before >= 0)
            continue;
        frame[fr].col_begin = av_malloc_array(ctx->height, sizeof(*frame->col_begin));
        if (!frame[fr].col_begin) {
            return NULL;
        }
        frame[fr].col_end   = av_malloc_array(ctx->height, sizeof(*frame->col_end));
        if (!frame[fr].col_end) {
            av_freep(&frame[fr].col_begin);
            return NULL;
        }
        for (uint32_t r = 0; r < ctx->height; r++) {
            av_assert1(pos < data->nb);
            frame[fr].col_begin[r] = data->b[pos];
            frame[fr].col_end[r] = data->e[pos];
            pos++;
        }
    }

    return src_ctx;
}

static void transform_frameshape_close(FLIF16TransformContext *ctx)
{
    TransformPrivFrameshape *data = ctx->priv_data;
    av_freep(&data->b);
    av_freep(&data->e);
}

static int transform_framecombine_init(FLIF16TransformContext *ctx,
                                       FLIF16RangesContext *src_ctx)
{
    TransformPrivFramecombine *data = ctx->priv_data;
    ff_flif16_chancecontext_init(&data->chancectx);
    return 1;
}

static void transform_framecombine_configure(FLIF16TransformContext *ctx,
                                             const int setting)
{
    TransformPrivFramecombine *data = ctx->priv_data;
    data->user_max_lookback = data->nb_frames = setting;
}

static int transform_framecombine_read(FLIF16TransformContext *ctx,
                                       FLIF16Context *dec_ctx,
                                       FLIF16RangesContext *src_ctx)
{
    TransformPrivFramecombine *data = ctx->priv_data;

    switch (ctx->i) {
    case 0:
        if (src_ctx->num_planes > 4)
            return 0;
        ctx->i = 1;

    case 1:
        RAC_GET(&dec_ctx->rc, &data->chancectx, 1, data->nb_frames - 1,
                &data->max_lookback, FLIF16_RAC_GNZ_INT);
    }

    ctx->i = 0;
    return 1;

need_more_data:
    return AVERROR(EAGAIN);
}

static FLIF16RangesContext *transform_framecombine_meta(FLIF16Context *ctx,
                                                        FLIF16PixelData *frame,
                                                        uint32_t frame_count,
                                                        FLIF16TransformContext *t_ctx,
                                                        FLIF16RangesContext *src_ctx)
{
    TransformPrivFramecombine *data = t_ctx->priv_data;
    RangesPrivFramecombine *rdata;
    FLIF16RangesContext *ranges;
    int lookback;

    ranges = av_mallocz(sizeof(*ranges));
    if (!ranges)
        return NULL;
    rdata = av_mallocz(sizeof(*rdata));
    if (!rdata) {
        av_free(ranges);
        return NULL;
    }
    av_assert0(data->max_lookback < frame_count);
    data->was_greyscale = (src_ctx->num_planes < 2);
    data->was_flat = (src_ctx->num_planes < 4);

    data->orig_num_planes = ctx->num_planes;
    ctx->num_planes = 5;

    lookback = frame_count - 1;
    if (lookback > data->max_lookback)
        lookback = data->max_lookback;

    ranges->r_no = FLIF16_RANGES_FRAMELOOKBACK;
    ranges->num_planes = 5;
    ranges->priv_data = rdata;

    rdata->num_prev_frames = lookback;
    rdata->alpha_min = (src_ctx->num_planes == 4 ? ff_flif16_ranges_min(src_ctx, 3) : 1);
    rdata->alpha_max = (src_ctx->num_planes == 4 ? ff_flif16_ranges_max(src_ctx, 3) : 1);
    rdata->ranges = src_ctx;

    return ranges;
}

static void transform_framecombine_reverse(FLIF16Context *ctx,
                                          FLIF16TransformContext *t_ctx,
                                          FLIF16PixelData *frame,
                                          uint32_t stride_row,
                                          uint32_t stride_col)
{
    TransformPrivFramecombine *data = t_ctx->priv_data;
    ctx->num_planes = data->orig_num_planes;
}

const FLIF16Transform flif16_transform_channelcompact = {
    .priv_data_size = sizeof(TransformPrivChannelcompact),
    .init           = &transform_channelcompact_init,
    .read           = &transform_channelcompact_read,
    .meta           = &transform_channelcompact_meta,
    .reverse        = &transform_channelcompact_reverse,
    .close          = &transform_channelcompact_close
};

const FLIF16Transform flif16_transform_ycocg = {
    .priv_data_size = sizeof(TransformPrivYCoCg),
    .init           = &transform_ycocg_init,
    .read           = NULL,
    .meta           = &transform_ycocg_meta,
    .reverse        = &transform_ycocg_reverse,
    .close          = NULL
};

const FLIF16Transform flif16_transform_permuteplanes = {
    .priv_data_size = sizeof(TransformPrivPermuteplanes),
    .init           = &transform_permuteplanes_init,
    .read           = &transform_permuteplanes_read,
    .meta           = &transform_permuteplanes_meta,
    .reverse        = &transform_permuteplanes_reverse,
    .close          = NULL
};

const FLIF16Transform flif16_transform_bounds = {
    .priv_data_size = sizeof(TransformPrivBounds),
    .init           = &transform_bounds_init,
    .read           = &transform_bounds_read,
    .meta           = &transform_bounds_meta,
    .reverse        = NULL,
    .close          = NULL
};

const FLIF16Transform flif16_transform_palette = {
    .priv_data_size = sizeof(TransformPrivPalette),
    .init           = &transform_palette_init,
    .read           = &transform_palette_read,
    .meta           = &transform_palette_meta,
    .reverse        = &transform_palette_reverse,
    .close          = &transform_palette_close
};

const FLIF16Transform flif16_transform_palettealpha = {
    .priv_data_size = sizeof(TransformPrivPalettealpha),
    .init           = &transform_palettealpha_init,
    .read           = &transform_palettealpha_read,
    .meta           = &transform_palettealpha_meta,
    .configure      = &transform_palettealpha_configure,
    .reverse        = &transform_palettealpha_reverse,
    .close          = &transform_palettealpha_close
};

const FLIF16Transform flif16_transform_colorbuckets = {
    .priv_data_size = sizeof(TransformPrivColorbuckets),
    .init           = &transform_colorbuckets_init,
    .read           = &transform_colorbuckets_read,
    .meta           = &transform_colorbuckets_meta,
    .close          = &transform_colorbuckets_close
};

const FLIF16Transform flif16_transform_framedup = {
    .priv_data_size = sizeof(TransformPrivFramedup),
    .init           = &transform_framedup_init,
    .read           = &transform_framedup_read,
    .meta           = &transform_framedup_meta,
    .configure      = &transform_framedup_configure,
    .close          = &transform_framedup_close
};

const FLIF16Transform flif16_transform_frameshape = {
    .priv_data_size = sizeof(TransformPrivFrameshape),
    .init           = &transform_frameshape_init,
    .read           = &transform_frameshape_read,
    .meta           = &transform_frameshape_meta,
    .configure      = &transform_frameshape_configure,
    .close          = &transform_frameshape_close
};

const FLIF16Transform flif16_transform_framecombine = {
    .priv_data_size = sizeof(TransformPrivFramecombine),
    .init           = &transform_framecombine_init,
    .read           = &transform_framecombine_read,
    .meta           = &transform_framecombine_meta,
    .configure      = &transform_framecombine_configure,
    .reverse        = &transform_framecombine_reverse,
    .close          = NULL
};

const FLIF16Transform *flif16_transforms[13] = {
    [FLIF16_TRANSFORM_CHANNELCOMPACT] = &flif16_transform_channelcompact,
    [FLIF16_TRANSFORM_YCOCG]          = &flif16_transform_ycocg,
    [FLIF16_TRANSFORM_RESERVED1]      = NULL,
    [FLIF16_TRANSFORM_PERMUTEPLANES]  = &flif16_transform_permuteplanes,
    [FLIF16_TRANSFORM_BOUNDS]         = &flif16_transform_bounds,
    [FLIF16_TRANSFORM_PALETTEALPHA]   = &flif16_transform_palettealpha,
    [FLIF16_TRANSFORM_PALETTE]        = &flif16_transform_palette,
    [FLIF16_TRANSFORM_COLORBUCKETS]   = &flif16_transform_colorbuckets,
    [FLIF16_TRANSFORM_RESERVED2]      = NULL,
    [FLIF16_TRANSFORM_RESERVED3]      = NULL,
    [FLIF16_TRANSFORM_DUPLICATEFRAME] = &flif16_transform_framedup,
    [FLIF16_TRANSFORM_FRAMESHAPE]     = &flif16_transform_frameshape,
    [FLIF16_TRANSFORM_FRAMELOOKBACK]  = &flif16_transform_framecombine
};

FLIF16TransformContext *ff_flif16_transform_init(int t_no, FLIF16RangesContext *r_ctx)
{
    const FLIF16Transform *trans;
    FLIF16TransformContext *ctx;
    void *k = NULL;

    trans = flif16_transforms[t_no];
    if (!trans)
        return NULL;
    ctx = av_mallocz(sizeof(*ctx));
    if (!ctx)
        return NULL;
    if (trans->priv_data_size) {
        k = av_mallocz(trans->priv_data_size);
        if (!k) {
            av_freep(&ctx);
            return NULL;
        }
    }
    ctx->t_no      = t_no;
    ctx->priv_data = k;
    ctx->segment   = 0;
    ctx->i         = 0;

    if (trans->init)
        if (!trans->init(ctx, r_ctx))
            return NULL;

    return ctx;
}

int ff_flif16_transform_read(FLIF16Context *dec_ctx,
                             FLIF16TransformContext *ctx,
                             FLIF16RangesContext *r_ctx)
{
    const FLIF16Transform *trans = flif16_transforms[ctx->t_no];
    if (trans->read)
        return trans->read(ctx, dec_ctx, r_ctx);
    else
        return 1;
}

FLIF16RangesContext *ff_flif16_transform_meta(FLIF16Context *ctx,
                                              FLIF16PixelData *frames,
                                              uint32_t frames_count,
                                              FLIF16TransformContext *t_ctx,
                                              FLIF16RangesContext *r_ctx)
{
    const FLIF16Transform *trans;
    trans = flif16_transforms[t_ctx->t_no];
    if (trans->meta)
        return trans->meta(ctx, frames, frames_count, t_ctx, r_ctx);
    else
        return r_ctx;
}

void ff_flif16_transform_configure(FLIF16TransformContext *ctx, const int setting)
{
    const FLIF16Transform *trans = flif16_transforms[ctx->t_no];
    if (trans->configure)
        trans->configure(ctx, setting);
}

void ff_flif16_transform_reverse(FLIF16Context *ctx, FLIF16TransformContext *t_ctx,
                                 FLIF16PixelData *frame, uint8_t stride_row,
                                 uint8_t stride_col)
{
    const FLIF16Transform *trans = flif16_transforms[t_ctx->t_no];
    if (trans->reverse != NULL)
        trans->reverse(ctx, t_ctx, frame, stride_row, stride_col);
}

void ff_flif16_transforms_close(FLIF16TransformContext *ctx)
{
    const FLIF16Transform *trans = flif16_transforms[ctx->t_no];
    if (trans->close)
        trans->close(ctx);
    if (trans->priv_data_size)
        av_freep(&ctx->priv_data);
    av_freep(&ctx);
}
