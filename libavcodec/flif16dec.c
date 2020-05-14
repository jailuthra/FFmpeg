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
#include "bytestream.h"

/*
 * Due to the nature of the format, the decoder has to take the entirety of the
 * data before it can generate any frames. The decoder has to return
 * AVERROR(EAGAIN) as long as the bitstream is incomplete.
 */

enum FLIF16States {
    FLIF16_HEADER = 1,
    FLIF16_SECONDHEADER,
    FLIF16_PIXELDATA,
    FLIF16_CHECKSUM
};

typedef struct FLIF16DecoderContext {
    GetByteContext *gb;  ///< Bytestream management context for bytestream2
    FLIF16RangeCoder *rc;
    // Transform  *tlist;  ///< Transform list
    int state;           ///< The section of the file the parser is in currently.
    unsigned int index;  ///< An index based on the current state. 
    
    // Primary Header
    uint8_t ia;          ///< Is image interlaced or/and animated or not
    uint8_t bpc;         ///< Bytes per channel
    uint8_t channels;    ///< Number of channels
    uint8_t varint;      ///< Number of varints to process in sequence
    
    // Secondary Header
    uint8_t  channelbpc; ///< bpc per channel. Size == 1 if bpc == '0' 
                         ///  else equal to number of frames
    
    // Flags. TODO Merge all these flags
    uint8_t alphazero;   ///< Alphazero
    uint8_t custombc;    ///< Custom Bitchance

    uint8_t cutoff; 
    uint8_t alphadiv;

    uint8_t loops;       ///< Number of times animation loops
    uint16_t *framedelay;///< Frame delay for each frame
    
    // Dimensions and other things.
    uint32_t width;
    uint32_t height;
    uint32_t frames;
    uint32_t meta;      ///< Size of a meta chunk
} FLIF16DecoderContext;


/*
 * TODO determine the minimum packet size
 */
static int flif16_read_header(AVCodecContext *avctx)
{
    uint8_t temp, count = 3;
    FLIF16DecoderContext *s = avctx->priv_data;
    // TODO Make do without this array
    uint32_t *vlist[] = { &s->width, &s->height, &s->frames };
    // Minimum size has empirically found to be 14 bytes.
    if (bytestream2_size(s->gb) < 14) {
        av_log(avctx, AV_LOG_ERROR, "buf size too small (%d)\n", 
               bytestream2_size(s->gb));
        return AVERROR(EINVAL);
    }
    
    if (bytestream2_get_be32(s->gb) != ((uint32_t) flif16_header)) {
        av_log(avctx, AV_LOG_ERROR, "bad magic number\n");
        return AVERROR(EINVAL);
    }
    
    s->state = FLIF16_HEADER;

    temp = bytestream2_get_byte(s->gb);
    s->ia       = temp >> 4;
    s->channels = (0xF0 & temp);
    s->bpc      = bytestream2_get_byte(s->gb);
    
    // Will be later updated by the secondary header step.
    s->channelbpc = (s->bpc == '1') ? 8 : 16;
    
    // Handle dimensions and frames
    for(int i = 0; i < 2 + ((s->ia > 4) ? 1 : 0); ++i) {
        while ((temp = bytestream2_get_byte(s->gb)) > 127) {
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
    (s->frames == 0) ? (s->frames = 1) : (s->frames += 2);
    
    // Handle Metadata Chunk. Currently it discards all data.

    while ((temp = bytestream2_get_byte(s->gb)) != 0) {
        bytestream2_seek(s->gb, 3, SEEK_CUR);
        // Read varint
        while ((temp = bytestream2_get_byte(s->gb)) > 127) {
            FF_FLIF16_VARINT_APPEND(s->meta, temp);
            if (!count) {
                av_log(avctx, AV_LOG_ERROR, "image dimensions too big\n");
                return AVERROR(ENOMEM);
            }
        }
        FF_FLIF16_VARINT_APPEND(s->meta, temp);
        bytestream2_seek(s->gb, s->meta, SEEK_CUR);
    }
    
    s->state = FLIF16_SECONDHEADER;
    return 0;
}

static int fli16_read_second_header(AVCodecContext *avctx)
{
    uint8_t temp;
    FLIF16DecoderContext *s = avctx->priv_data;
    s->rc = ff_flif16_rac_init(s->gb);
    
    // In original source this is handled in what seems a very bogus manner. 
    // It takes all the bpps of all channels and takes the max.
    if (s->bpc == '0')
        for(uint8_t i = 0; i < s->channels; ++i) 
            s->channelbpc = ff_flif16_rac_read_uni_int(s->rc, 1, 16);
    
    if (s->channels > 3)
        s->alphazero = ff_flif16_rac_read_uni_int(s->rc, 0, 1);
    
    if (s->frames > 1) {
        s->loops = ff_flif16_rac_read_uni_int(s->rc, 0, 100);
        for (uint32_t i = 0; i < s->frames; i++)
            s->framedelay[i] = ff_flif16_rac_read_uni_int(s->rc, 0, 60000);
    }
    
    // Has custom alpha flag
    temp = ff_flif16_rac_read_uni_int(s->rc, 0, 1);
    
    if (temp) {
        s->cutoff   = ff_flif16_rac_read_uni_int(s->rc, 1, 128);
        s->alphadiv = ff_flif16_rac_read_uni_int(s->rc, 2, 128);
        s->custombc = ff_flif16_rac_read_uni_int(s->rc, 0, 1);
    }
    
    if (s->custombc) {
        av_log(avctx, AV_LOG_ERROR, "custom bitchances not implemented\n");
        return AVERROR_PATCHWELCOME;
    }
    
    return AVERROR_EOF; // We are testing upto this point
    
    // Transformations
    while (ff_flif16_rac_read_bit(s->rc)) {
        temp = ff_flif16_rac_read_uni_int(s->rc, 0, 13); // Transform number
        // ff_flif16_transform_process(temp, tlist, rc);
    }
    return 0;
}

static int flif16_read_pixeldata(AVCodecContext *avctx, AVFrame *p)
{
    return AVERROR_EOF;
}

static int flif16_read_checksum(AVCodecContext *avctx)
{
    return AVERROR_EOF;
}

// TODO Add all Functions
static int flif16_decode_frame(AVCodecContext *avctx,
                               void *data, int *got_frame,
                               AVPacket *avpkt)
{
    printf("[Decode]\n");
    int ret = AVERROR(EINVAL);
    
    FLIF16DecoderContext *s = avctx->priv_data;
    const uint8_t *buf      = avpkt->data;
    int buf_size            = avpkt->size;
    AVFrame *p              = data;
    
    bytestream2_init(s->gb, buf, buf_size);
    
    // Looping is done to change states in between functions.
    // Function will either exit on AVERROR(EAGAIN) or AVERROR_EOF
    do {
        switch(s->state) {
            case 0: case FLIF16_HEADER:
                ret = flif16_read_header(avctx);
                break;
            
            case FLIF16_SECONDHEADER:
                ret = fli16_read_second_header(avctx);
                break;

            case FLIF16_PIXELDATA:
                ret = flif16_read_pixeldata(avctx, p);
                break;

            case FLIF16_CHECKSUM:
                ret = flif16_read_checksum(avctx);
                break;
        }
    } while (!ret);

    printf("Result:\n"                        \
           "Width: %u, Height: %u, Frames: %u"\
           "ia: %x bpc: %c channels: %u\n"    \
           "channelbpc: %u\n"                 \
           "alphazero: %u custombc: %u\n"     \
           "cutoff: %u alphadiv: %u \n"       \
           "loops: %u\n", s->width, s->height, s->frames, s->ia, s->bpc, 
           s->channels, s->channelbpc, s->alphazero, s->custombc, s->cutoff,
           s->alphadiv, s->loops);
    return ret;
}

static av_cold int flif16_decode_end(AVCodecContext *avctx)
{
    FLIF16DecoderContext *s = avctx->priv_data;
    av_free(s->rc);
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
