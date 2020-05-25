#include "flif16_transform.h"
#include "flif16_rangecoder.h"
#include "libavutil/common.h"

FLIF16Transform* process(int t_no, FLIF16DecoderContext *s){
    FLIF16Transform *t = flif16_transforms[t_no];
    t->transform_ctx = (FLIF16TransformContext*)
                        av_mallocz(sizeof(FLIF16TransformContext));
    t->transform_ctx->dec_ctx = s;
    t->read(t->transform_ctx);
    return t;
}

uint8_t ff_flif16_transform_ycocg_read(FLIF16TransformContext *ctx){
    ctx->priv_data_size = sizeof(int) + sizeof(FLIF16ColorRanges);
    ctx->priv_data = av_mallocz(ctx->priv_data_size);
    FLIF16ColorRanges *ranges = (FLIF16ColorRanges *)(ctx->priv_data
                                                    + sizeof(int));
    ranges->num_planes = ctx->dec_ctx->channels;
    return 1;
}

uint8_t ff_flif16_transform_ycocg_init(FLIF16TransformContext *ctx, 
                                       FLIF16ColorRanges *srcRanges){
    if(srcRanges->num_planes < 3) 
        return 0;
    
    if(   srcRanges->min[0] == srcRanges->max[0] 
       || srcRanges->min[1] == srcRanges->max[1] 
       || srcRanges->min[2] == srcRanges->max[2])
        return 0;
    
    if(   srcRanges->min[0] < 1 
       || srcRanges->min[1] < 1 
       || srcRanges->min[2] < 1) 
        return 0;

    int *origmax4 = (int *) ctx->priv_data;
    *origmax4 = FF_MAX3(srcRanges->max[0], 
                    srcRanges->max[1], 
                        srcRanges->max[2])/4 -1;

    FLIF16ColorRanges *ranges = (FLIF16ColorRanges *) (ctx->priv_data
                                                     + sizeof(int));
    int p;
    for (p = 0; p < ranges->num_planes; p++) {
        ranges->max[p] = srcRanges->max[p];
        ranges->min[p] = srcRanges->min[p];
    }
    return 1;
}

uint8_t ff_flif16_transform_ycocg_forward(FLIF16TransformContext *ctx,
                                          FLIF16InterimPixelData * pixelData){
    int r, c;
    FLIF16ColorVal R,G,B,Y,Co,Cg;

    int height = pixelData->height;
    int width = pixelData->width;

    for (r=0; r<height; r++) {
        for (c=0; c<width; c++) {
            R = pixelData->data[0][r*width + c];
            G = pixelData->data[1][r*width + c];
            B = pixelData->data[2][r*width + c];

            Y = (((R + B)>>1) + G)>>1;
            Co = R - B;
            Cg = G - ((R + B)>>1);

            pixelData->data[0][r*width + c] = Y;
            pixelData->data[1][r*width + c] = Co;
            pixelData->data[2][r*width + c] = Cg;
        }
    }

    int *origmax4 = (int *)ctx->priv_data;
    FLIF16ColorRanges *ranges = (FLIF16ColorRanges *)(ctx->priv_data
                                                    + sizeof(int));
    // Will see if ranges need to be stored separately in transform struct only.
    int p;
    for (p = 0; p < ranges->num_planes; p++) {
        ranges->max[p] = ff_max_range_ycocg(p, *origmax4);
        ranges->min[p] = ff_min_range_ycocg(p, *origmax4);
    }

//I don't know yet what to do with this flag in forward and reverse transforms.
    ctx->done = 1;
    return 1;
}

uint8_t ff_flif16_transform_ycocg_reverse(FLIF16TransformContext *ctx,
                                          FLIF16InterimPixelData * pixelData){
    int r, c;
    FLIF16ColorVal R,G,B,Y,Co,Cg;

    int height = pixelData->height;
    int width = pixelData->width;

    for (r=0; r<height; r++) {
        for (c=0; c<width; c++) {
            Y = pixelData->data[0][r*width + c];
            Co= pixelData->data[1][r*width + c];
            Cg= pixelData->data[2][r*width + c];
  
            R = Co + Y + ((1-Cg)>>1) - (Co>>1);
            G = Y - ((-Cg)>>1);
            B = Y + ((1-Cg)>>1) - (Co>>1);

            pixelData->data[0][r*width + c] = R;
            pixelData->data[1][r*width + c] = G;
            pixelData->data[2][r*width + c] = B;
        }
    }
    return 0;
}

uint8_t ff_flif16_transform_permuteplanes_read(FLIF16TransformContext * ctx){
    ctx->priv_data_size = 6*sizeof(uint8_t) + sizeof(FLIF16ColorRanges);
    ctx->priv_data = av_mallocz(ctx->priv_data_size);
    uint8_t *subtract = (uint8_t *)ctx->priv_data;
    uint8_t *permutation = (uint8_t *)(ctx->priv_data + sizeof(uint8_t));
    FLIF16ColorRanges *ranges = (FLIF16ColorRanges *)(ctx->priv_data 
                                                + 6*sizeof(uint8_t));
    ranges->num_planes = ctx->dec_ctx->channels;
    RangeCoder* rac = ctx->dec_ctx->rc;
    *subtract = ff_flif16_rac_read_nz_int(rac, 0, 1);
    uint8_t from[4] = {0, 0, 0, 0}, to[4] = {0, 0, 0, 0};
    int p;
    int planes = ranges->num_planes;
    for(p = 0; p < planes; p++){
    permutation[p] = ff_flif16_rac_read_nz_int(rac, 0, planes-1);
    from[p] = 1;
        to[p] = 1;
    }
    for(p = 0; p < planes; p++){
        if(!from[p] || !to[p]){
            return 0;
    }
    }
    return 1;
}

uint8_t ff_flif16_transform_permuteplanes_init(FLIF16TransformContext *ctx, 
                                               FLIF16ColorRanges *srcRanges){
    if(srcRanges->num_planes < 3)
    return 0;
    if(srcRanges->min[0] < 1  || srcRanges->min[1] < 1 || srcRanges->min[2] < 1) 
        return 0;
    FLIF16ColorRanges *ranges = (FLIF16ColorRanges *) (ctx->priv_data 
                                                     + sizeof(int));
    int p;
    for(p = 0; p < ranges->num_planes; p++){
    ranges->max[p] = srcRanges->max[p];
    ranges->min[p] = srcRanges->min[p];
    }
    return 1;
}

uint8_t ff_flif16_transform_permuteplanes_forward(
                                        FLIF16TransformContext *ctx,
                                        FLIF16InterimPixelData * pixelData){
//PENDING WORK
//Actual forward transform is yet to be written
    uint8_t *subtract = (uint8_t *)ctx->priv_data;
    uint8_t *permutation = (uint8_t *)(ctx->priv_data + sizeof(uint8_t));
    FLIF16ColorRanges *ranges = (FLIF16ColorRanges *)(ctx->priv_data 
                                                    + 6*sizeof(uint8_t));
    ranges->num_planes = ctx->dec_ctx->channels;

    if (*subtract) {
        if (ranges->num_planes > 3) {
            ranges->min[3] = ranges->min[permutation[3]];
            ranges->max[3] = ranges->max[permutation[3]];
        }

        ranges->max[1] = ranges->max[1] - ranges->min[0];
        ranges->min[1] = ranges->min[1] - ranges->max[0];
        ranges->max[2] = ranges->max[2] - ranges->min[0];
        ranges->min[2] = ranges->min[2] - ranges->max[0];
    }
    return 1;
}
