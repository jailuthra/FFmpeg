#include <stdint.h>
#include "../libavutil/frame.h"
#define MAX_PLANES 5

typedef int16_t ColorVal;

typedef struct{
   	ColorVal min, max;
}ColorRanges;

typedef struct{
    char* desc;
    int transform_number;
    uint8_t done;
}Transform;

typedef struct{
    int origmax4;
    ColorRanges *ranges;
}TransformYCoCg;

typedef struct{
    uint8_t initialized;
    int height, width;
    int num_planes;
    ColorVal *data[MAX_PLANES];
}interimPixelData;

typedef struct TransformPermute{
	uint8_t subtract;
	uint8_t permutation[MAX_PLANES];
    ColorRanges *ranges;
}TransformPermute; 

ColorRanges* getRanges(interimPixelData* pixelData);

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

int process(Transform transform, AVFrame *frame, int p, interimPixelData *pixelData);
TransformYCoCg initYCoCg(interimPixelData *pixelData);
int processYCoCg(interimPixelData *pixelData);
int invProcessYCoCg(AVFrame *frame);

