#include "transformflif16.h"

int process(Transform transform, AVFrame *frame, int p){
    switch(transform.transform_number){
        case 1: //Transform number for YCoCg transformation is 1 officially.
        if(!transform.done){
			TransformYCoCg transformYCoCg = initYCoCg(frame, p);
            if(!processYCoCg(frame))
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

TransformYCoCg initYCoCg(AVFrame *frame, int p){
	TransformYCoCg transform;
	transform.num_planes = p;
	transform.ranges = getRanges(transform.num_planes, frame);
    transform.origmax4 = max(transform.ranges[0].max, transform.ranges[1].max, transform.ranges[2].max)/4 -1;
    for(p=0; p<transform.num_planes; p++){
    	transform.ranges[p].max = max_range_YCoCg(p, transform.origmax4);
    	transform.ranges[p].min = min_range_YCoCg(p, transform.origmax4);
	}
    return transform;
}

int processYCoCg(AVFrame *frame){
    int r, c;
    ColorVal R,G,B,Y,Co,Cg;
    
    //Assuming all the channels will have same width and height
	int height = frame[0].height;
    int width = frame[0].width;
    
	for (r=0; r<height; r++) {
        for (c=0; c<width; c++) {
            R = frame->data[0][r*width + c];
            G = frame->data[1][r*width + c];
            B = frame->data[2][r*width + c];

            Y = (((R + B)>>1) + G)>>1;
            Co = R - B;
            Cg = G - ((R + B)>>1);

            frame->data[0][r*width + c] = Y;
            frame->data[1][r*width + c] = Co;
            frame->data[2][r*width + c] = Cg;
        }
    }
	return 0;
}

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

ColorRanges* getRanges(int p, AVFrame *frame){
    ColorRanges ranges[p];
    int i, c, r, width, height;
    uint8_t min, max;
    
    for(i=0; i<p; i++){
        width = frame[p].width;
        height = frame[p].height;
        min = frame->data[p][0];
        max = frame->data[p][0];
        for(r=0; r<frame[p].height; r++){
            for(c=0; c<frame[p].width; r++){
                if(min > frame->data[p][r*width + c])
                    min = frame->data[p][r*width + c];
                if(max < frame->data[p][r*width + c])
                    max = frame->data[p][r*width + c];
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
