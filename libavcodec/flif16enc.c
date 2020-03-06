#include "flif16.h"
#include "avcodec.h"

// TODO Add all Functions
static int flif16_encode_frame(AVCodecContext *avctx, AVPacket *pkt,
                            const AVFrame *pict, int *got_packet)
{
    return -1;
}

AVCodec ff_flif16_encoder = {
    .name           = "flif16",
    .long_name      = NULL_IF_CONFIG_SMALL("FLIF (Free Lossless Image Format)"),
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_FLIF16,
    .priv_data_size = 0,
    //.init           = NULL,
    .encode2        = flif16_encode_frame,
    //.close          = NULL,
    //.pix_fmts       = (const enum AVPixelFormat[]){},
    .priv_class     = NULL,
};
