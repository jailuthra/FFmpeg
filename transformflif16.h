#include <stdint.h>
#include "../libavutil/frame.h"

typedef int32_t ColorVal;

typedef struct ColorRanges{
   	ColorVal min, max;
}ColorRanges;

typedef struct Transform{
    char* desc;
    int transform_number;
    uint8_t done;
}Transform;

typedef struct TransformYCoCg{
    int origmax4;
    ColorRanges *ranges;
    int num_planes;
}TransformYCoCg;

ColorRanges* getRanges(int p, AVFrame *frame);

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

ColorRanges crangesYCoCg(int p, ColorVal* prevPlanes, TransformYCoCg transform);

int process(Transform transform, AVFrame *frame, int p);
TransformYCoCg initYCoCg(AVFrame *frame, int p);
int processYCoCg(AVFrame *frame);
int invProcessYCoCg(AVFrame *frame);

