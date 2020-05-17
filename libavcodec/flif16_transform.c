#include "flif16_transform.h"

int process(Transform transform, AVFrame *frame, int planes, interimPixelData *pixelData){
	if(!pixelData->initialized){
		int r, c;
		int p;
		int width = frame->width, height = frame->height;
		pixelData->height = height;
		pixelData->width = width;
		pixelData->num_planes = planes;
		for(p = 0; p < p; p++){
			pixelData->data[p] = (ColorVal*)malloc(height*width*sizeof(ColorVal));
			for(r = 0; r < frame->height; r++){
				for(c = 0; c < frame->width; c++){
					pixelData->data[p][r*width + c] = frame->data[p][r*width + c];
				}
			}
		}
		pixelData->initialized = 1;
	}
	switch(transform.transform_number){
        case 1: //Transform number for YCoCg transformation is 1 officially.
        if(!transform.done){
			TransformYCoCg transformYCoCg = initYCoCg(pixelData);
            if(!processYCoCg(pixelData))
            	transform.done = 1;
            return 0;
        }
		break;

        /*
        Rest of the cases will be implemented here.
        */

        default:
        break;
    }
    return 0;
}

TransformYCoCg initYCoCg(interimPixelData *pixelData){
	TransformYCoCg transform;
	transform.ranges = getRanges(pixelData);
    transform.origmax4 = max(transform.ranges[0].max, transform.ranges[1].max, transform.ranges[2].max)/4 -1;
    int p;
	for(p=0; p<pixelData->num_planes; p++){
    	transform.ranges[p].max = max_range_YCoCg(p, transform.origmax4);
    	transform.ranges[p].min = min_range_YCoCg(p, transform.origmax4);
	}
    return transform;
}

int processYCoCg(interimPixelData *pixelData){
    int r, c;
    ColorVal R,G,B,Y,Co,Cg;
    
    //Assuming all the channels will have same width and height
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
	return 0;
}

// TODO inversetransform requires more work. 
/*
int invProcessYCoCg(AVFrame *frame){
	int r, c;
    ColorVal R,G,B,Y,Co,Cg;
    
    //Assuming all the channels will have same width and height
	int height = frame[0].height;
    int width = frame[0].width;
    
	for (r=0; r<height; r++) {
        for (c=0; c<width; c++) {
            Y = frame->data[0][r*width + c];
            Co= frame->data[1][r*width + c];
            Cg= frame->data[2][r*width + c];

            R = Co + Y + ((1-Cg)>>1) - (Co>>1);
            G = Y - ((-Cg)>>1);
            B = Y + ((1-Cg)>>1) - (Co>>1);

            frame->data[0][r*width + c] = R;
            frame->data[1][r*width + c] = G;
            frame->data[2][r*width + c] = B;
        }
    }
	return 0;
}
*/

ColorRanges* getRanges(interimPixelData *pixelData){
    int p = pixelData->num_planes;
	ColorRanges ranges[p];
    int i, c, r, width, height;
    ColorVal min, max;
    
    for(i=0; i<p; i++){
        width = pixelData->width;
        height = pixelData->height;
        min = pixelData->data[p][0];
        max = pixelData->data[p][0];
        for(r=0; r<pixelData->height; r++){
            for(c=0; c<pixelData->width; r++){
                if(min > pixelData->data[p][r*width + c])
                    min = pixelData->data[p][r*width + c];
                if(max < pixelData->data[p][r*width + c])
                    max = pixelData->data[p][r*width + c];
            }
        }
        ranges[p].min = min;
        ranges[p].max = max;
    }
    return ranges;
}

int max(int a, int b, int c){
	if(a > b){
		if(a > c)
			return a;
		else
			return c;
	}
	else
		return b;
}

int min(int a, int b){
	if(a < b)
		return a;
	else
		return b;
}

int get_min_y(int origmax4){
	return 0;
}

int get_max_y(int origmax4){
	return 4*origmax4-1;
}

int get_min_co(int origmax4, int yval){
	int newmax = 4*origmax4 - 1;
	if (yval < origmax4-1)
    	return -3 - 4*yval; 
	else if (yval >= 3*origmax4)
      	return 4*(yval - newmax);
    else
      	return -newmax;
}

int get_max_co(int origmax4, int yval){
	int newmax = 4*origmax4 - 1;
	if (yval < origmax4-1)
    	return 3 + 4*yval; 
	else if (yval >= 3*origmax4)
      	return 4*(newmax - yval);
    else
      	return newmax;
}

int get_min_cg(int origmax4, int yval, int coval){
	int newmax = 4*origmax4 - 1;
	if (yval < origmax4-1)
    	return -2 - 2*yval; 
	else if (yval >= 3*origmax4)
      	return -2*(newmax-yval) + 2*((abs(coval)+1)/2);
    else{
      	return min(2*yval + 1, 2*newmax - 2*yval - 2*abs(coval)+1)/2;
	}
}

int get_max_cg(int origmax4, int yval, int coval){
	int newmax = 4*origmax4 - 1;
	if (yval < origmax4-1)
    	return 1 + 2*yval - 2*(abs(coval)/2); 
	else if (yval >= 3*origmax4)
      	return 2 * (newmax-yval);
    else
      	return min(2*(yval- newmax), -2*yval - 1 + 2*(abs(coval)/2));
}

int min_range_YCoCg(int p, int origmax4){
	switch(p){
		case 0:
			return 0;
        case 1:
			return -4*origmax4+1;
        case 2:
			return -4*origmax4+1;
		default:
			return 0;
			break;
	}
}

int max_range_YCoCg(int p, int origmax4){
	switch(p){
		case 0:
			return 0;
        case 1:
			return 4*origmax4+1;
        case 2:
			return 4*origmax4+1;
		default:
			return 0;
			break;
	}
}

ColorRanges crangeYCoCg(int p, ColorVal* prevPlanes, TransformYCoCg transform){
	ColorRanges crange;
	switch(p){
		case 0:
			crange.min = get_min_y(transform.origmax4);
			crange.max = get_max_y(transform.origmax4);
			break;
		case 1:
			crange.min = get_min_co(transform.origmax4, prevPlanes[0]);
			crange.max = get_max_co(transform.origmax4, prevPlanes[0]);
			break;	
		case 2:
			crange.min = get_min_cg(transform.origmax4, prevPlanes[0], prevPlanes[1]);
			crange.max = get_max_cg(transform.origmax4, prevPlanes[0], prevPlanes[1]);
			break;
		default:
			break; 
	}
	return crange;
}
