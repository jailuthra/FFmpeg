/*
 * FLIF16 Decoder
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
 * FLIF16 Decoder
*/

#include <stdio.h> // Remove

#include "flif16.h"
#include "flif16_rangecoder.h"

#include "avcodec.h"
#include "libavutil/common.h"
#include "bytestream.h"

/*
 * Due to the nature of the format, the decoder has to take the entirety of the
 * data before it can generate any frames. The decoder has to return
 * AVERROR(EAGAIN) as long as the bitstream is incomplete.
 */

// Static property values
static const int properties_ni_rgb_size[] = {7, 8, 9, 7, 7};
static const int properties_ni_rgba_size[] = {8, 9, 10, 7, 7};


// The order in which the planes are encoded.
// FRA (Lookback) (animations-only, value refers to a previous frame) has
// to be first, because all other planes are not encoded if lookback != 0
// Alpha has to be next, because for fully transparent A=0 pixels, the other
// planes are not encoded
// Y (luma) is next (the first channel for still opaque images), because it is
// perceptually most important
// Co and Cg are in that order because Co is perceptually slightly more
// important than Cg [citation needed]
const int plane_ordering[] = {4,3,0,1,2}; // FRA (lookback), A, Y, Co, Cg

enum FLIF16States {
    FLIF16_HEADER = 1,
    FLIF16_SECONDHEADER,
    FLIF16_TRANSFORM,
    FLIF16_MANIAC,
    FLIF16_PIXELDATA,
    FLIF16_CHECKSUM
};

static int flif16_read_header(AVCodecContext *avctx)
{
    uint8_t temp, count = 3;
    FLIF16DecoderContext *s = avctx->priv_data;
    // TODO Make do without this array
    uint32_t *vlist[] = { &s->width, &s->height, &s->frames };
    // Minimum size has empirically found to be 8 bytes.

    s->cut   = CHANCETABLE_DEFAULT_CUT;
    s->alpha = CHANCETABLE_DEFAULT_ALPHA;

    if (bytestream2_size(&s->gb) < 8) {
        av_log(avctx, AV_LOG_ERROR, "buf size too small (%d)\n",
               bytestream2_size(&s->gb));
        return AVERROR(EINVAL);
    }

    if (bytestream2_get_le32(&s->gb) != (*((uint32_t *) flif16_header))) {
        av_log(avctx, AV_LOG_ERROR, "bad magic number\n");
        return AVERROR(EINVAL);
    }

    s->state = FLIF16_HEADER;

    temp = bytestream2_get_byte(&s->gb);
    s->ia       = temp >> 4;
    s->channels = (0x0F & temp);

    if (!(s->ia % 2)) {
        av_log(avctx, AV_LOG_ERROR, "interlaced images not supported\n");
        return AVERROR_PATCHWELCOME;
    }
    
    s->bpc      = bytestream2_get_byte(&s->gb);

    // Handle dimensions and frames
    for(int i = 0; i < 2 + ((s->ia > 4) ? 1 : 0); ++i) {
        while ((temp = bytestream2_get_byte(&s->gb)) > 127) {
            FF_FLIF16_VARINT_APPEND(*vlist[i], temp);
            if (!count) {
                av_log(avctx, AV_LOG_ERROR, "image dimensions too big\n");
                return AVERROR(ENOMEM);
            }
        }
        FF_FLIF16_VARINT_APPEND(*vlist[i], temp);
        count = 3;
    }

    s->width++;
    s->height++;
    (s->ia > 4) ? (s->frames += 2) : (s->frames = 1);

    // Handle Metadata Chunk. Currently it discards all data.

    while ((temp = bytestream2_get_byte(&s->gb)) != 0) {
        bytestream2_seek(&s->gb, 3, SEEK_CUR);
        // Read varint
        while ((temp = bytestream2_get_byte(&s->gb)) > 127) {
            FF_FLIF16_VARINT_APPEND(s->meta, temp);
            if (!count) {
                av_log(avctx, AV_LOG_ERROR, "metadata chunk too big \n");
                return AVERROR(ENOMEM);
            }
        }
        FF_FLIF16_VARINT_APPEND(s->meta, temp);
        bytestream2_seek(&s->gb, s->meta, SEEK_CUR);
    }

    printf("[%s] left = %d\n", __func__, bytestream2_get_bytes_left(&s->gb));
    s->state = FLIF16_SECONDHEADER;
    return 0;
}

static int flif16_read_second_header(AVCodecContext *avctx)
{
    uint32_t temp;
    FLIF16DecoderContext *s = avctx->priv_data;

    switch (s->segment) {
        case 0:
            s->buf_count += bytestream2_get_buffer(&s->gb, s->buf + s->buf_count,
                                               FFMIN(bytestream2_get_bytes_left(&s->gb),
                                               (FLIF16_RAC_MAX_RANGE_BYTES - s->buf_count)));
            // MSG("s->buf_count = %d buf = ", s->buf_count);
            for(int i = 0; i < FLIF16_RAC_MAX_RANGE_BYTES; ++i)
                printf("%x ", s->buf[i]);
            printf("\n");
            if (s->buf_count < FLIF16_RAC_MAX_RANGE_BYTES)
                return AVERROR(EAGAIN);

            ff_flif16_rac_init(&s->rc, &s->gb, s->buf, s->buf_count);

        case 1:
            // In original source this is handled in what seems to be a very
            // bogus manner. It takes all the bpps of all channels and then
            // takes the max.
            if (s->bpc == '0') {
                s->bpc = 0;
                for (; s->i < s->channels; ++s->i) {
                    RAC_GET(&s->rc, NULL, 1, 15, &temp, FLIF16_RAC_UNI_INT);
                    s->bpc = FFMAX(s->bpc, (1 << temp) - 1);
                }
            } else
                s->bpc = (s->bpc == '1') ? 255 : 65535;
            s->i = 0;
            s->range = ff_flif16_ranges_static_init(s->channels, s->bpc);
            // MSG("channels : %d & bpc : %d\n", s->channels, s->bpc);

        case 2:
            if (s->channels > 3)
                RAC_GET(&s->rc, NULL, 0, 1, (uint32_t *) &s->alphazero,
                        FLIF16_RAC_UNI_INT);
            ++s->segment;

        case 3:
            if (s->frames > 1) {
                RAC_GET(&s->rc, NULL, 0, 100, (uint32_t *) &s->loops,
                        FLIF16_RAC_UNI_INT);
                s->framedelay = av_mallocz(sizeof(*(s->framedelay)) * s->frames);
            }
            ++s->segment;

        case 4:
            // MSG("s->segment = %d\n", s->segment);
            if (s->frames > 1) {
                for (; (s->i) < (s->frames); ++(s->i)) {
                    RAC_GET(&s->rc, NULL, 0, 60000, &(s->framedelay[(s->i)]),
                            FLIF16_RAC_UNI_INT);
                }
                s->i = 0;
            }
            ++s->segment;

        case 5:
            // Has custom alpha flag
            RAC_GET(&s->rc, NULL, 0, 1, &s->customalpha, FLIF16_RAC_UNI_INT);
            // MSG("has_custom_cutoff_alpha = %d\n", s->customalpha);
            ++s->segment;

        case 6:
            if (s->customalpha)
                RAC_GET(&s->rc, NULL, 1, 128, &s->cut, FLIF16_RAC_UNI_INT);
            ++s->segment;

        case 7:
            if (s->customalpha) {
                RAC_GET(&s->rc, NULL, 2, 128, &s->alpha, FLIF16_RAC_UNI_INT);
                s->alpha = 0xFFFFFFFF / s->alpha;
            }
            ++s->segment;

        case 8:
            if (s->customalpha)
                RAC_GET(&s->rc, NULL, 0, 1, &s->custombc, FLIF16_RAC_UNI_INT);
            if (s->custombc) {
                av_log(avctx, AV_LOG_ERROR, "custom bitchances not implemented\n");
                return AVERROR_PATCHWELCOME;
            }
            goto end;
    }

    end:
    s->state   = FLIF16_TRANSFORM;
    s->segment = 0;

    #ifdef MULTISCALE_CHANCES_ENABLED
    s->rc->mct = ff_flif16_multiscale_chancetable_init();
    ff_flif16_build_log4k_table(&s->rc->log4k);
    #endif

    s->segment = 0;
    ff_flif16_chancetable_init(&s->rc.ct, s->alpha, s->cut);
    return 0;

    need_more_data:
    // MSG("Need more data\n");
    return AVERROR(EAGAIN);
}


static int flif16_read_transforms(AVCodecContext *avctx)
{
    FLIF16DecoderContext *s = avctx->priv_data;
    FLIF16RangesContext *prev_range;
    uint8_t temp;

    loop:
    switch (s->segment) {
        case 0:
            RAC_GET(&s->rc, NULL, 0, 0, &temp, FLIF16_RAC_BIT);
            if(!temp){
                //return AVERROR_EOF;
                goto end;
            }
            ++s->segment;

        case 1:
            RAC_GET(&s->rc, NULL, 0, 13, &temp, FLIF16_RAC_UNI_INT);
            printf("Transform : %d\n", temp);
            if (!flif16_transforms[temp]) {
                av_log(avctx, AV_LOG_ERROR, "transform %u not implemented\n", temp);
                return AVERROR_PATCHWELCOME;
            }
            s->transforms[s->transform_top] = ff_flif16_transform_init(temp, s->range);
            if(!s->transforms[s->transform_top])
                return AVERROR_EXIT;
            //printf("%d\n", s->transforms[s->transform_top]->t_no);
            if(!ff_flif16_transform_read(s->transforms[s->transform_top], s, s->range))
                return AVERROR_EXIT;
            prev_range = s->range;
            s->range = ff_flif16_transform_meta(s->transforms[s->transform_top], prev_range);
            if(!s->range)
                return AVERROR_EXIT;
            printf("Ranges : %d\n", s->range->r_no);
            s->segment = 0;
            ++s->transform_top;
            goto loop;

        case 2:
            end:
            s->segment = 2;
            printf("[Resultant Ranges]\n");
            for(int i = 0; i < 5; ++i)
                printf("%d: %d, %d\n", i, ff_flif16_ranges_min(s->range, i),
                ff_flif16_ranges_max(s->range, i));
                
            // Read invisible pixel predictor
            if ( s->alphazero && s->channels > 3
                && ff_flif16_ranges_min(s->range, 3) <= 0
                && !(s->ia % 2))
                RAC_GET(&s->rc, NULL, 0, 2, &s->ipp, FLIF16_RAC_UNI_INT);
    }

    s->segment = 0;
    s->state  = FLIF16_MANIAC;
    return 0;

    need_more_data:
    return AVERROR(EAGAIN);
}

static int flif16_read_maniac_forest(AVCodecContext *avctx)
{
    int ret;
    FLIF16DecoderContext *s = avctx->priv_data;
    printf("called\n");
    if (!s->maniac_ctx.forest) {
        s->maniac_ctx.forest = av_mallocz((s->channels) * sizeof(*(s->maniac_ctx.forest)));
        if (!s->maniac_ctx.forest) {
            av_log(avctx, AV_LOG_ERROR, "could not allocate \n");
            return AVERROR(ENOMEM);
        }
        s->segment = s->i = 0; // Remove later
    }

    switch (s->segment) {
        case 0:
            loop:
            printf("channel: %d\n", s->i);
            if (s->i >= s->channels)
                goto end;
            s->prop_ranges = ff_flif16_maniac_ni_prop_ranges_init(&s->prop_ranges_size, s->range,
                                                                  s->i, s->channels);
            printf("Prop ranges:\n");
            for(int i = 0; i < s->prop_ranges_size; ++i)
                printf("(%d, %d)\n", s->prop_ranges[i][0], s->prop_ranges[i][1]);
            if(!s->prop_ranges)
                return AVERROR(ENOMEM);
            ++s->segment;

        case 1:
            if (ff_flif16_ranges_min(s->range, s->i) >= ff_flif16_ranges_max(s->range, s->i)) {
                ++s->i;
                goto loop;
            }
            ret = ff_flif16_read_maniac_tree(&s->rc, &s->maniac_ctx, s->prop_ranges,
                                             s->prop_ranges_size, s->i);
            printf("Ret: %d\n", ret);
            if (ret)
                goto need_more_data;
            av_freep(&s->prop_ranges);
            --s->segment;
            ++s->i;
            goto loop;
    }

    end:
    s->state = FLIF16_PIXELDATA;
    s->segment = 0;
    return ret;

    need_more_data:
    return ret;
}

static FLIF16ColorVal flif16_ni_predict_calcprops(FLIF16PixelData *pixel,
                                                  FLIF16ColorVal *properties,
                                                  FLIF16RangesContext *ranges_ctx,
                                                  int p,
                                                  uint32_t r,
                                                  uint32_t c,
                                                  FLIF16ColorVal *min,
                                                  FLIF16ColorVal *max,
                                                  const FLIF16ColorVal fallback,
                                                  uint8_t nobordercases)
{
    FLIF16ColorVal guess, left, top, topleft, gradientTL;
    int width = pixel->width, height = pixel->height;
    int which = 0;
    int index=0;
    if (p < 3) {
        for (int pp = 0; pp < p; pp++) {
            properties[index++] = ff_flif16_pixel_get(pixel, pp, r, c); //image(pp,r,c);
        }
        if (ranges_ctx->num_planes>3)
            properties[index++] = ff_flif16_pixel_get(pixel, 3, r, c); //image(3,r,c);
    }
    left = (nobordercases || c>0 ? ff_flif16_pixel_get(pixel, p, r, c-1) : 
           (r > 0 ? ff_flif16_pixel_get(pixel, p, r-1, c) : fallback));
    top = (nobordercases || r>0 ? ff_flif16_pixel_get(pixel, p, r-1, c) : left);
    topleft = (nobordercases || (r>0 && c>0) ? 
              ff_flif16_pixel_get(pixel, p, r-1, c-1) : (r > 0 ? top : left));
    gradientTL = left + top - topleft;
    guess = MEDIAN3(gradientTL, left, top);
    ff_flif16_ranges_snap(ranges_ctx, p, properties, min, max, &guess);
    assert(min >= ranges->min(ranges_ctx, p));
    assert(max <= ranges->max(ranges_ctx, p));
    assert(guess >= min);
    assert(guess <= max);
    if (guess == gradientTL) which = 0;
    else if (guess == left) which = 1;
    else if (guess == top) which = 2;

    properties[index++] = guess;
    properties[index++] = which;

    if (nobordercases || (c > 0 && r > 0)){ 
        properties[index++] = left - topleft;
        properties[index++] = topleft - top;
    }
    else{ 
        properties[index++] = 0;
        properties[index++] = 0; 
    }

    if (nobordercases || (c+1 < width && r > 0))
        properties[index++] = top - ff_flif16_pixel_get(pixel, p, r-1, c+1); // top - topright 
    else
        properties[index++] = 0;
    if (nobordercases || r > 1)
        properties[index++] = ff_flif16_pixel_get(pixel, p, r-2, c) - top;  // toptop - top
    else 
        properties[index++] = 0;
    if (nobordercases || c > 1)
        properties[index++] = ff_flif16_pixel_get(pixel, p, r, c-2) - left;  // leftleft - left
    else 
        properties[index++] = 0;
    return guess;
}

static inline FLIF16ColorVal flif16_ni_predict(FLIF16PixelData *pixel,
                                               uint32_t p, uint32_t r, uint32_t c,
                                               FLIF16ColorVal gray) {
    FLIF16ColorVal left = (c>0 ? ff_flif16_pixel_get(pixel, p, r, c-1) : (r > 0 ? ff_flif16_pixel_get(pixel, p, r-1, c) : gray));
    FLIF16ColorVal top = (r>0 ? ff_flif16_pixel_get(pixel, p, r-1, c) : left);
    FLIF16ColorVal topleft = (r>0 && c>0 ? ff_flif16_pixel_get(pixel, p, r-1, c-1) : top);
    FLIF16ColorVal gradientTL = left + top - topleft;
    return MEDIAN3(gradientTL, left, top);
}

static int flif16_read_ni_plane(FLIF16DecoderContext *s,
                                FLIF16RangesContext *ranges_ctx,
                                FLIF16ColorVal *properties,
                                uint8_t p,
                                uint32_t fr,
                                uint32_t r,
                                FLIF16ColorVal gray,
                                FLIF16ColorVal minP,
                                uint8_t lookback)
{
    // TODO write in a position independent manner
    FLIF16ColorVal min, max;
    FLIF16ColorVal curr, guess;
    uint32_t begin = 0, end = s->width;

    switch (s->segment2) {
        case 0:
            // if this is a duplicate frame, copy the row from the frame being duplicated
            if (s->out_frames[fr].seen_before >= 0) {
                ff_flif16_copy_rows(&s->out_frames[fr], &s->out_frames[fr - 1], p, r, 0, s->width);
                return 0;
            }
            // if this is not the first or only frame, fill the beginning of the row
            // before the actual pixel data
            if (fr > 0) {
                //if alphazero is on, fill with a predicted value, otherwise
                // copy pixels from the previous frame
                // begin = image.col_begin[r];
                // end = image.col_end[r];
                if (s->alphazero && p < 3) {
                    for (uint32_t c = 0; c < begin; c++)
                        if (ff_flif16_pixel_get(&s->out_frames[fr], 3, r, c) == 0)
                            ff_flif16_pixel_set(&s->out_frames[fr], p, r, c, flif16_ni_predict(&s->out_frames[fr], p, r, c, gray));
                        else
                            ff_flif16_pixel_set(&s->out_frames[fr], p, r, c,
                                                ff_flif16_pixel_get(&s->out_frames[fr - 1], p, r, c)); 
                } else if (p!=4) {
                    ff_flif16_copy_rows(&s->out_frames[fr], &s->out_frames[fr - 1], p, r, 0, begin);
                }
            }
            ++s->segment;

        if (r > 1 && !lookback && begin == 0 && end > 3) {
        case 1:
        //decode actual pixel data
            s->c = begin;
            for (; s->c < 2; s->c++) {
                if (s->alphazero && p<3 && ff_flif16_pixel_get(&s->out_frames[fr], 3, r, s->c) == 0) {
                    ff_flif16_pixel_set(&s->out_frames[fr], p, r, s->c, flif16_ni_predict(&s->out_frames[fr], p, r, s->c, gray));
                    continue;
                }
                guess = flif16_ni_predict_calcprops(&s->out_frames[fr], properties, ranges_ctx, p, r, s->c, &min, &max, minP, 0);
                ++s->segment2;
        case 2:
                // FLIF16ColorVal curr = coder.read_int(properties, min - guess, max - guess) + guess;
                MANIAC_GET(&s->rc, &s->maniac_ctx, properties, p, min - guess, max - guess, &curr);
                curr += guess;
                ff_flif16_pixel_set(&s->out_frames[fr], p, r, s->c, curr);
                --s->segment2;
            }
            s->segment2 += 2;

        case 3:
            for (; s->c < end-1; s->c++) {
                if (s->alphazero && p < 3 && ff_flif16_pixel_get(&s->out_frames[fr], 3, r, s->c) == 0) {
                    ff_flif16_pixel_set(&s->out_frames[fr], p, r, s->c, flif16_ni_predict(&s->out_frames[fr], p, r, s->c, gray));
                    continue;
                }
                guess = flif16_ni_predict_calcprops(&s->out_frames[fr], properties, ranges_ctx, p, r, s->c, &min, &max, minP, 1);
                 ++s->segment2;
        case 4:
                // FLIF16ColorVal curr = coder.read_int(properties, min - guess, max - guess) + guess;
                MANIAC_GET(&s->rc, &s->maniac_ctx, properties, p, min - guess, max - guess, &curr);
                curr += guess;
                ff_flif16_pixel_set(&s->out_frames[fr], p, r, s->c, curr);
                 --s->segment2;
            }
            s->segment2 += 2;

        case 5:
            for (; s->c < end; s->c++) {
                if (s->alphazero && p < 3 && ff_flif16_pixel_get(&s->out_frames[fr], 3, r, s->c) == 0) {
                    ff_flif16_pixel_set(&s->out_frames[fr], p, r, s->c, flif16_ni_predict(&s->out_frames[fr], p, r, s->c, gray));
                    continue;
                }
               guess = flif16_ni_predict_calcprops(&s->out_frames[fr], properties, ranges_ctx, p, r, s->c, &min, &max, minP, 0);
                ++s->segment2;
        case 6:
                // FLIF16ColorVal curr = coder.read_int(properties, min - guess, max - guess) + guess;
                MANIAC_GET(&s->rc, &s->maniac_ctx, properties, p, min - guess, max - guess, &curr);
                curr += guess;
                ff_flif16_pixel_set(&s->out_frames[fr], p, r, s->c, curr);
                --s->segment2;
            }
            s->segment2 += 2;

        } else {

        case 7:
            s->segment2 = 7;
            for (s->c = begin; s->c < end; s->c++) {
                //predict pixel for alphazero and get a previous pixel for FRA
                if (s->alphazero && p < 3 && ff_flif16_pixel_get(&s->out_frames[fr], 3, r, s->c) == 0) {
                    ff_flif16_pixel_set(&s->out_frames[fr], p, r, s->c, flif16_ni_predict(&s->out_frames[fr], p, r, s->c, gray));
                    continue;
                }

                if (lookback && p < 4 && ff_flif16_pixel_get(&s->out_frames[fr], 4, r, s->c) > 0) {
                    assert(fr >= ff_flif16_pixel_get(&s->out_frames[fr], 4, r, s->c));
                    ff_flif16_pixel_set(&s->out_frames[fr], p, r, s->c,
                    ff_flif16_pixel_get((&s->out_frames[fr - ff_flif16_pixel_get(&s->out_frames[fr], 4, r, s->c)]), p, r, s->c));
                    //Remove these comments after testing
                    //Above line 541-542 emulating line 544                                       
                    //plane.set(r,c,images[fr-image.getFRA(r,c)](p,r,c));

                //calculate properties and use them to decode the next pixel
                    guess = flif16_ni_predict_calcprops(&s->out_frames[fr], properties, ranges_ctx, p, r, s->c, &min, &max, minP, 0);
                    if (lookback && p == 4 && max > fr)
                        max = fr;
                    ++s->segment2;
        case 8:
                    MANIAC_GET(&s->rc, &s->maniac_ctx, properties, p, min - guess, max - guess, &curr);
                    curr += guess;
                    ff_flif16_pixel_set(&s->out_frames[fr], p, r, s->c, curr);
                    --s->segment2;
                }
            }
        } /* end if */

        // If this is not the first or only frame, fill the end of the row after the actual pixel data
        if (fr > 0) {
            //if alphazero is on, fill with a predicted value, otherwise copy pixels from the previous frame
            if (s->alphazero && p < 3) {
                for (uint32_t c = end; c < s->width; c++)
                    if (ff_flif16_pixel_get(&s->out_frames[fr], 3, r, s->c) == 0)
                        ff_flif16_pixel_set(&s->out_frames[fr], p, r, s->c, flif16_ni_predict(&s->out_frames[fr], p, r, s->c, gray));
                    else
                        ff_flif16_pixel_set(&s->out_frames[fr], p, r, s->c, ff_flif16_pixel_get(&s->out_frames[fr - 1], p, r, s->c));
            } else if(p != 4) {
                 ff_flif16_copy_rows(&s->out_frames[fr], &s->out_frames[fr - 1], p, r, end, s->width);
            }
        }
    }

    return 0;

    need_more_data:
    return AVERROR(EAGAIN);
}


static inline FLIF16ColorVal *flif16_compute_grays(FLIF16RangesContext *ranges)
{
    FLIF16ColorVal *grays; // a pixel with values in the middle of the bounds
    grays = av_malloc(ranges->num_planes * sizeof(*grays));
    for (int p = 0; p < ranges->num_planes; p++)
        grays[p] = (ff_flif16_ranges_min(ranges, p) + ff_flif16_ranges_max(ranges, p)) / 2;
    return grays;
}



static int flif16_read_ni_image(AVCodecContext *avctx)
{
    FLIF16DecoderContext *s = avctx->priv_data;
    int ret;
    FLIF16ColorVal min_p;
    /*
    for (int p = 0; p < images[0].numPlanes(); p++) {
        Ranges propRanges;
        initPropRanges_scanlines(propRanges, *ranges, p);
        coders.emplace_back(rac, propRanges, forest[p], 0, options.cutoff, options.alpha);
    }*/

    // The FinalPropertySymbolCoder does not use the propranges at any point of time.
    // Only the size of propranges is used, which can by calculated in a single
    // line copypasted from flif16.c. Not even that is necessary. Therefore this
    // is completely useless.

    // To read MANIAC integers, do:
    // ff_flif16_maniac_read_int(s->rc, s->maniac_ctx, properties, plane, min, max, &target)
    // Or something like that. Check out the definition in flif16_rangecoder.c

    // Set images to gray
    switch (s->segment) {
        case 0:
            for (int p = 0; p < s->range->num_planes; p++) {
                if (ff_flif16_ranges_min(s->range, p) < ff_flif16_ranges_max(s->range, p))
                    for (uint32_t fr = 0; fr < s->frames; fr++) {
                        for (uint32_t r = 0; r < s->height; r++) { // Handle the 2 pixels per frame stuff here.
                            for (uint32_t c = 0; c < s->width; c++) {
                                ff_flif16_pixel_set(&s->out_frames[fr], p, r, c,
                                                    (ff_flif16_ranges_min(s->range, p) +
                                                     ff_flif16_ranges_max(s->range, p)) / 2);
                            }
                        }
                    }
            }

            s->grays = flif16_compute_grays(s->range); // free later
            s->i = s->i2 = s->i3 = 0;
            ++s->segment;
            
            for (; s->i < 5; ++s->i) {
                s->curr_plane = plane_ordering[s->i];
                if (s->curr_plane >= s->channels)
                    continue;
                if (ff_flif16_ranges_min(s->range, s->curr_plane) <
                    ff_flif16_ranges_max(s->range, s->curr_plane))
                    continue;
                s->properties = av_mallocz((s->channels > 3 ? properties_ni_rgba_size[s->curr_plane]:
                                                              properties_ni_rgb_size[s->curr_plane]));
                for (; s->i2 < s->height; s->i2++) {
                    for (; s->i3 < s->frames; s->i3++) {
        case 1:
                        min_p = ff_flif16_ranges_min(s->range, s->curr_plane);
                        ret = flif16_read_ni_plane(s, s->range, s->properties,
                                                   s->curr_plane,
                                                   s->i3,
                                                   s->i2,
                                                   s->grays[s->curr_plane],
                                                   min_p,
                                                   0);
                        if (ret)
                            goto need_more_data;
                    }
                }
                av_freep(&s->properties);
            }
        } // End switch

    need_more_data:
    return ret;
}


static int flif16_read_pixeldata(AVCodecContext *avctx, AVFrame *p)
{
    FLIF16DecoderContext *s = avctx->priv_data;
    s->out_frames = ff_flif16_frames_init(s->frames, s->channels, 32,
                                          s->width, s->height);
    if (!s->out_frames)
        return AVERROR(ENOMEM);

    if(s->ia % 2)
        return flif16_read_ni_image(avctx);
    else
        return AVERROR_EOF;
}

static void flif16_write_frame(AVCodecContext *avctx, AVFrame *out_frame,
                               uint32_t frame_no)
{
    // Refer to libavcodec/bmp.c for an example.
    // ff_set_dimensions(avctx, width, height );
    // avctx->pix_fmt = ...
    // if ((ret = ff_get_buffer(avctx, p, 0)) < 0)
    //     return ret;
    // p->pict_type = AV_PICTURE_TYPE_I;
    // p->key_frame = 1;
    // for(...)
    //     p->data[...] = ...
}

static int flif16_read_checksum(AVCodecContext *avctx)
{
    return AVERROR_EOF;
}

static int flif16_decode_frame(AVCodecContext *avctx,
                               void *data, int *got_frame,
                               AVPacket *avpkt)
{
    int ret = AVERROR(EINVAL);
    FLIF16DecoderContext *s = avctx->priv_data;
    const uint8_t *buf      = avpkt->data;
    int buf_size            = avpkt->size;
    AVFrame *p              = data;
    // MSG("Packet Size = %d\n", buf_size);
    bytestream2_init(&s->gb, buf, buf_size);

    // Looping is done to change states in between functions.
    // Function will either exit on AVERROR(EAGAIN) or AVERROR_EOF
    do {
        switch(s->state) {
            case FLIF16_HEADER:
                ret = flif16_read_header(avctx);
                break;

            case FLIF16_SECONDHEADER:
                ret = flif16_read_second_header(avctx);
                break;

            case FLIF16_TRANSFORM:
                ret = flif16_read_transforms(avctx);
                break;

            case FLIF16_MANIAC:
                ret = flif16_read_maniac_forest(avctx);
                break;

            case FLIF16_PIXELDATA:
                ret = flif16_read_pixeldata(avctx, p);
                break;

            case FLIF16_CHECKSUM:
                ret = flif16_read_checksum(avctx);
                break;
        }
    } while (!ret);

    printf("[Decode Result]\n"                  \
           "Width: %u, Height: %u, Frames: %u\n"\
           "ia: %x bpc: %u channels: %u\n"      \
           "alphazero: %u custombc: %u\n"       \
           "cutoff: %u alphadiv: %u \n"         \
           "loops: %u\nl", s->width, s->height, s->frames, s->ia, s->bpc,
           s->channels, s->alphazero, s->custombc, s->cut,
           s->alpha, s->loops);

    if (s->framedelay) {
        printf("Framedelays:\n");
        for(uint32_t i = 0; i < s->frames; ++i)
            printf("%u, ", s->framedelay[i]);
        printf("\n");
    }

   if(s->maniac_ctx.forest) {
        printf("Tree Size: %d\n", s->maniac_ctx.forest[0]->size);
        printf("MANIAC Tree first node:\n" \
               "property value: %d\n", s->maniac_ctx.forest[0]->data[0].property);
    }
    return ret;
}

static av_cold int flif16_decode_end(AVCodecContext *avctx)
{
    // TODO complete function
    FLIF16DecoderContext *s = avctx->priv_data;
    if(s->framedelay)
        av_freep(&s->framedelay);
    if (s->prop_ranges)
        av_freep(&s->prop_ranges);
    return 0;
}

AVCodec ff_flif16_decoder = {
    .name           = "flif16",
    .long_name      = NULL_IF_CONFIG_SMALL("FLIF (Free Lossless Image Format)"),
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_FLIF16,
    .close          = flif16_decode_end,
    .priv_data_size = sizeof(FLIF16DecoderContext),
    .decode         = flif16_decode_frame,
    //.capabilities   = 0,
    //.caps_internal  = 0,
    .priv_class     = NULL,
};
