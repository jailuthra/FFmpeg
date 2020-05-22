#include <stdint.h>
#include "../libavutil/frame.h"
//#include "flif16dec.h"
#define MAX_PLANES 5

typedef int16_t ColorVal;

typedef struct{
   	ColorVal min[MAX_PLANES], max[MAX_PLANES];
    int num_planes;
}ColorRanges;

char* TransDesc[13] = {"ChannelCompact", "YCoCg", "?", "Permute", "Bounds", "PaletteAlpha", "Palette", "ColorBuckets", "?", "?", "DuplicateFrame", "FrameShape", "FrameLookback"};

typedef struct Transform{
    char* desc;                     //Description of transform
    uint8_t transform_number;       
    uint8_t done;
    FLIF16DecoderContext *s;

    int data_size;
    void *data;
}Transform;

typedef struct{
    uint8_t initialized;            //FLAG : initialized or not.
    int height, width;
    ColorVal *data[MAX_PLANES];
    ColorRanges ranges;
}interimPixelData;

ColorRanges* getRanges(interimPixelData* pixelData, ColorRanges *ranges);

int getmax(ColorRanges* ranges, int p);

int max_range_YCoCg(int p, int origmax4);
int min_range_YCoCg(int p, int origmax4);

int get_min_y(int);
int get_max_y(int orgimax4);

int get_min_co(int orgimax4, int yval);
int get_max_co(int orgimax4, int yval);

int get_min_cg(int orgimax4, int yval, int coval);
int get_max_cg(int orgimax4, int yval, int coval);

int min(int, int);
int max(int, int, int);

//ColorRanges crangesYCoCg(int p, ColorVal* prevPlanes, TransformYCoCg transform);

Transform* process(int t_no, FLIF16DecoderContext *s);

uint8_t transform_read(Transform *transform);
uint8_t transform_init(Transform *transform, ColorRanges *ranges);
uint8_t transform_forward(Transform *transform, interimPixelData *pixelData);
uint8_t transform_reverse(Transform *transform, interimPixelData *pixelData);

