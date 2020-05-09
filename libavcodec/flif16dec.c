/*
 * FLIF16 Decoder
 * Copyright (c) 2003 Fabrice Bellard
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
#include "flif16_rac.h"

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
    FLIFF16_PIXELDATA,
    FLIF16_CHECKSUM
}

typedef struct FLIF16DecoderContext {
    GetByteContext *gb; ///< Bytestream management context for bytestream2
    int state;          ///< The section of the file the parser is in currently.
    unsigned int index; ///< An index based on the current state. 
    uint8_t animated;   ///< Is image animated or not
    uint8_t varint;     ///< Number of varints to process in sequence
    uint64_t width;
    uint64_t height;
    uint64_t frames;
    uint64_t meta;      ///< Size of a meta chunk
} FLIF16DecoderContext;

static int flif16_read_header(AVCodecContext *avctx)
{
    FLIF16DecoderContext *s = avctx->priv_data;
    // Here we are assuming that the buffer is big enough to at least hold the
    // Magic number,
    if (buf_size < 4) {
        av_log(avctx, AV_LOG_ERROR, "buf size too small (%d)\n", buf_size);
        return AVERROR(EINVAL);
    }
    
    if (bytestream_get_be32(s->gb) != (uint32_t) flif16_header) {
        av_log(avctx, AV_LOG_ERROR, "bad magic number\n");
        return AVERROR(EINVAL);
    }
    
    s->state = FLIF16_HEADER;

    /*
     * TODO put header values into struct. (???)
     */
}

static int fli16_read_second_header(AVCodecContext *avctx)
{
    
}

static int flif16_read_pixeldata(AVCodecContext *avctx, AVFrame *p)
{
    
}

static int flif16_read_checksum(AVCodecContext *avctx)
{
    
}

// TODO Add all Functions
static int flif16_decode_frame(AVCodecContext *avctx,
                               void *data, int *got_frame,
                               AVPacket *avpkt)
{
    printf("#Decode\n");
    int ret;
    
    FLIF16DecoderContext *s =  avctx->priv_data;
    const uint8_t *buf = avpkt->data;
    int buf_size       = avpkt->size;
    AVFrame *p         = data;
    
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
    } while (!ret)

    return ret;
}

AVCodec ff_flif16_decoder = {
    .name           = "flif16",
    .long_name      = NULL_IF_CONFIG_SMALL("FLIF (Free Lossless Image Format)"),
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_FLIF16,
    .init           = flif16_decode_init,
    .priv_data_size = sizeof(FLIF16DecoderContext),
    .decode         = flif16_decode_frame,
    //.capabilities   = 0,
    //.caps_internal  = 0,
    .priv_class     = NULL,
};
