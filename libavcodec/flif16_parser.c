/*
 * FLIF16 parser
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
  * FLIF16 parser
  */

#include "flif16.h"
#include "parser.h"
#include "libavutil/avassert.h"
#include "libavutil/bswap.h"

#include <stdio.h> //remove
#include <stdint.h>
#include <stdlib.h>

static int flif16_parse(AVCodecParserContext *s, AVCodecContext *avctx,
                        const uint8_t **poutbuf, int *poutbuf_size,
                        const uint8_t *buf, int buf_size)
{
    // The bitstream does not have a determinate endpoint. The only thing we
    // can do in the parser is pass the block to the decoder.
    __PLN__
    /*if (ff_combine_frame(NULL, buf_size - 1, &buf, &buf_size) < 0) {
        *poutbuf      = NULL;
        *poutbuf_size = 0;
        return buf_size;
    }*/
    printf("[%s] Packet Size = %d\n", __func__, buf_size);
    *poutbuf      = buf;
    *poutbuf_size = buf_size;
    return buf_size;
}

AVCodecParser ff_flif16_parser = {
    .codec_ids      = { AV_CODEC_ID_FLIF16 },
    .priv_data_size = 0,
    .parser_parse   = flif16_parse,
    .parser_close   = ff_parse_close,
};

