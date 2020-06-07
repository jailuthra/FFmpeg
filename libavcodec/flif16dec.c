/*
 * FLIF16 Decoder
 * Copyright (c) 2020 Anamitra Ghorui
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


// TODO prefix approprate functions with ff_*

enum FLIF16States {
    FLIF16_HEADER = 1,
    FLIF16_SECONDHEADER,
    FLIF16_TRANSFORM,
    FLIF16_MANIAC,
    FLIF16_PIXELDATA,
    FLIF16_CHECKSUM
};

static int ff_flif16_read_header(AVCodecContext *avctx)
{
    uint8_t temp, count = 3;
    FLIF16DecoderContext *s = avctx->priv_data;
    // TODO Make do without this array
    uint32_t *vlist[] = { &s->width, &s->height, &s->frames };
    // Minimum size has empirically found to be 8 bytes.

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
    __PLN__
    s->width++;
    s->height++;
    (s->ia > 4) ? (s->frames += 2) : (s->frames = 1);

    // Handle Metadata Chunk. Currently it discards all data.
    __PLN__
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

static int ff_flif16_read_second_header(AVCodecContext *avctx)
{
    uint32_t temp;
    FLIF16DecoderContext *s = avctx->priv_data;

    if (!s->rc) {
        s->buf_count += bytestream2_get_buffer(&s->gb, s->buf,
                                       FFMIN(bytestream2_get_bytes_left(&s->gb),
                                       (FLIF16_RAC_MAX_RANGE_BYTES -
                                       s->buf_count)));
        if (s->buf_count < FLIF16_RAC_MAX_RANGE_BYTES)
            return AVERROR(EAGAIN);

        s->rc = ff_flif16_rac_init(&s->gb, s->buf, s->buf_count);
    }

    switch (s->segment) {
        case 0:
            // In original source this is handled in what seems to be a very
            // bogus manner. It takes all the bpps of all channels and then
            // takes the max.
            if (s->bpc == '0') {
                s->bpc = 0;
                for (; s->i < s->channels; ++s->i) {
                    RAC_GET(s->rc, NULL, 1, 15, &temp, FLIF16_RAC_UNI_INT);
                    s->bpc = FFMAX(s->bpc, (1 << temp) - 1);
                }
            } else
                s->bpc = (s->bpc == '1') ? 255 : 65535;
            s->i = 0;

            s->ranges = av_malloc(s->channels * sizeof(*(s->ranges)));
            for (int i = 0; i < s->channels; ++i)
                RANGE_SET(s->ranges[i], 0, s->bpc);
            //for(int i = 0; i < s->channels; ++i)
            //    s->src_ranges->max[i] = s->bpc;
            ++s->segment; __PLN__

        case 1:
            if (s->channels > 3)
                RAC_GET(s->rc, NULL, 0, 1, (uint32_t *) &s->alphazero,
                        FLIF16_RAC_UNI_INT);
            ++s->segment; __PLN__

        case 2:
            if (s->frames > 1) {
                RAC_GET(s->rc, NULL, 0, 100, (uint32_t *) &s->loops,
                        FLIF16_RAC_UNI_INT);
                s->framedelay = av_mallocz(sizeof(*(s->framedelay)) * s->frames);
            }
            ++s->segment; __PLN__

        case 3:
            if (s->frames > 1) {
                for (; (s->i) < (s->frames); ++(s->i)) {
                    RAC_GET(s->rc, NULL, 0, 60000, &(s->framedelay[(s->i)]),
                            FLIF16_RAC_UNI_INT);
                }
                s->i = 0;
            }
            ++s->segment;

        case 4:
            // Has custom alpha flag
            RAC_GET(s->rc, NULL, 0, 1, &temp, FLIF16_RAC_UNI_INT);
            printf("[%s] has_custom_cutoff_alpha = %d\n", __func__, temp);
            ++s->segment;

        case 5:
            if (temp)
                RAC_GET(s->rc, NULL, 1, 128, &s->cutoff, FLIF16_RAC_UNI_INT);
            ++s->segment;

        case 6:
            if (temp)
                RAC_GET(s->rc, NULL, 2, 128, &s->alphadiv, FLIF16_RAC_UNI_INT);
            ++s->segment;

        case 7:
            if (temp)
                RAC_GET(s->rc, NULL, 0, 1, &s->cutoff, FLIF16_RAC_UNI_INT);
            if (s->custombc) {
                av_log(avctx, AV_LOG_ERROR,
                       "custom bitchances not implemented\n");
                return AVERROR_PATCHWELCOME;
            }
            goto end;
    }

    end:
    s->state   = FLIF16_TRANSFORM;
    s->segment = 0;
    return AVERROR_EOF; // Remove this when testing out transforms.
    return 0;

    need_more_data:
    printf("[%s] Need more data\n", __func__);
    return AVERROR(EAGAIN);
}


static int ff_flif16_read_transforms(AVCodecContext *avctx)
{
    FLIF16DecoderContext *s = avctx->priv_data;
    uint32_t temp;

    start:
    switch (s->segment) {
        case 0:
            RAC_GET(s->rc, NULL, 0, 0, &temp, FLIF16_RAC_BIT);
            if(!temp)
                goto end;
            // Make a pointer array. Do something like:
            // s->tlist[++s->tlist_top] = ff_flif16_transform_init(temp, ...)
            // tlist is an array of pointers of FLIF16TransformContexts
            ++s->segment;

        case 1:
            // ff_flif16_transform_read(s->tlist[s->tlist_top], ...)
            s->segment = 0;
            goto start;
    }

    end:
    return 0;

    need_more_data:
    return AVERROR(EAGAIN);
}

static int ff_flif16_read_maniac_tree(AVCodecContext *avctx)
{
    FLIF16DecoderContext *s = avctx->priv_data;
    FLIF16MANIACNode *curr_node;
    FLIF16MANIACStack *curr_stack;
    FLIF16RangeCoder *rc = s->rc;
    FLIF16MANIACContext *m = &s->maniac_ctx;
    int p, oldmin, oldmax, split_val;

    if (!m->tree_size) {
        m->tree  = av_malloc(MANIAC_TREE_BASE_SIZE * sizeof(*(m->tree)));
        m->stack = av_malloc(MANIAC_TREE_BASE_SIZE * sizeof(*(m->stack)));
        if(!((m->tree) && (m->stack)))
            return AVERROR(ENOMEM);
        for(int i = 0; i < 3; ++i)
            m->ctx[i] = ff_flif16_chancecontext_init();
        m->stack_top = m->tree_top = 0;
        m->tree_size = MANIAC_TREE_BASE_SIZE;
        m->stack[m->stack_top].id   = 0;
        m->stack[m->stack_top].mode = 0;
        ++m->stack_top;
    }

    switch (rc->segment) {
        case 0:
            start:
            if(!m->stack_top)
                goto end;
            curr_stack = &m->stack[m->stack_top - 1];
            curr_node  = &m->tree[curr_stack->id];
            p = curr_stack->p;

            if(!curr_stack->visited){
                switch (curr_stack->mode) {
                    case 1:
                        RANGE_SET(s->prop_ranges[p], curr_stack->min,
                                  curr_stack->max);
                        break;

                    case 2:
                        s->prop_ranges[p][0] = curr_stack->min;
                        break;
                }
            } else {
                s->prop_ranges[p][1] = curr_stack->max2;
                --m->stack_top;
                goto start;
            }
            curr_stack->visited = 1;
            ++rc->segment;

        case 1:
            // int p = tree[stack[top]].property = coder[0].read_int2(0,nb_properties) - 1;
            RAC_GET(rc, m->ctx[0], 0, s->prop_ranges_size, &curr_node->property,
                    FLIF16_RAC_GNZ_INT);
            p = --(curr_node->property);

            if (p == -1) {
                --m->stack_top;
                goto start;
            }

            oldmin = s->prop_ranges[p][0];
            oldmax = s->prop_ranges[p][1];
            if (oldmin >= oldmax)
                return AVERROR(EINVAL);
            ++rc->segment;

        case 2:
            //tree[stack[top]].count = coder[1].read_int2(CONTEXT_TREE_MIN_COUNT,
            //                                            CONTEXT_TREE_MAX_COUNT);
            RAC_GET(rc, m->ctx[1], MANIAC_TREE_MIN_COUNT, MANIAC_TREE_MAX_COUNT,
                    &curr_node->count, FLIF16_RAC_GNZ_INT);
            ++rc->segment;

        case 3:
            // int splitval = n.splitval = coder[2].read_int2(oldmin, oldmax-1);
            RAC_GET(rc, m->ctx[2], oldmin, oldmax-1, &curr_node->split_val,
                    FLIF16_RAC_GNZ_INT);
            split_val = curr_node->split_val;
            ++rc->segment;

        case 4:
            if ((m->tree_top) >= m->tree_size) {
                av_realloc(m->tree, (m->tree_size) * 2);
                if(!(m->tree))
                    return AVERROR(ENOMEM);
            }
            m->tree_size *= 2;

            if((m->stack_top) >= m->stack_size) {
                av_realloc(m->stack, (m->stack_size) * 2);
                if(!(m->stack))
                    return AVERROR(ENOMEM);
            }
            m->stack_size *= 2;

            // WHEN GOING BACK UP THE TREE
            curr_stack->max2 = oldmax;

            // PUSH 1
            // <= splitval
            // subrange[p].first = oldmin;
            // subrange[p].second = splitval;
            // if (!read_subtree(childID + 1, subrange, tree)) return false;
            // TRAVERSE CURR + 2 (RIGHT CHILD)
            m->stack[m->stack_top].id      = m->tree_top + 1;
            m->stack[m->stack_top].p       = p;
            m->stack[m->stack_top].min     = oldmin;
            m->stack[m->stack_top].max     = split_val;
            m->stack[m->stack_top].mode    = 1;
            m->stack[m->stack_top].visited = 0;
            ++m->stack_top;

            // PUSH 2
            // > splitval
            // subrange[p].first = splitval+1;
            // if (!read_subtree(childID, subrange, tree)) return false;
            // TRAVERSE CURR + 1 (LEFT CHILD)
            m->stack[m->stack_top].id      = m->tree_top;
            m->stack[m->stack_top].p       = p;
            m->stack[m->stack_top].min     = oldmin;
            m->stack[m->stack_top].mode    = 2;
            m->stack[m->stack_top].visited = 0;
            ++m->stack_top;

            m->tree_top += 2;

            goto start;
    }

    end:
    av_free(m->stack);
    for(int i = 0; i < 3; ++i)
        av_free(m->ctx[i]);
    return 1;

    need_more_data:
    return AVERROR(EAGAIN);
}


static int ff_flif16_read_pixeldata(AVCodecContext *avctx, AVFrame *p)
{
    return AVERROR_EOF;
}

static int ff_flif16_read_checksum(AVCodecContext *avctx)
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
    printf("[Decode] Packet Size = %d\n", buf_size);
    bytestream2_init(&s->gb, buf, buf_size);
    __PLN__
    // Looping is done to change states in between functions.
    // Function will either exit on AVERROR(EAGAIN) or AVERROR_EOF
    do {
        switch(s->state) {
            case 0: case FLIF16_HEADER:
                ret = ff_flif16_read_header(avctx);
                break;

            case FLIF16_SECONDHEADER:
                ret = ff_flif16_read_second_header(avctx);
                break;

            case FLIF16_TRANSFORM:
                ret = ff_flif16_read_transforms(avctx);
                break;

            case FLIF16_MANIAC:
                ret = ff_flif16_read_maniac_tree(avctx);
                break;

            case FLIF16_PIXELDATA:
                __PLN__
                ret = ff_flif16_read_pixeldata(avctx, p);
                break;

            case FLIF16_CHECKSUM:
                ret = ff_flif16_read_checksum(avctx);
                break;
        }
    } while (!ret);

    printf("[Decode Result]\n"                  \
           "Width: %u, Height: %u, Frames: %u\n"\
           "ia: %x bpc: %u channels: %u\n"      \
           "alphazero: %u custombc: %u\n"       \
           "cutoff: %u alphadiv: %u \n"         \
           "loops: %u\n", s->width, s->height, s->frames, s->ia, s->bpc,
           s->channels, s->alphazero, s->custombc, s->cutoff,
           s->alphadiv, s->loops);
    if (s->framedelay) {
        printf("Framedelays:\n");
        for(uint32_t i = 0; i < s->frames; ++i)
            printf("%u, ", s->framedelay[i]);
        printf("\n");
    }
    return ret;
}

static av_cold int flif16_decode_end(AVCodecContext *avctx)
{
    FLIF16DecoderContext *s = avctx->priv_data;
    av_free(s->rc);
    if(s->framedelay)
        av_free(s->framedelay);
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
