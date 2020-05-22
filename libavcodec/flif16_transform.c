#include "flif16_transform.h"
#include "flif16_rangecoder.h"

Transform* process(char* desc, FLIF16DecoderContext *s){
    Transform* transform;
	if(desc == "YCoCg"){
		transform = (Transform*)malloc(sizeof(Transform));
		transform->s = s;
		transform->desc = desc;
		transform->transform_number = 1;
		transform_read(transform);
	}
	else if(desc == "Permute"){
		transform = (Transform*)malloc(sizeof(Transform));
		transform->s = s;
		transform->desc = desc;
		transform->transform_number = 3;
		transform_read(transform);
	}
    return transform;
}

uint8_t transform_read(Transform *transform){
    if(transform->desc == "YCoCg"){
		//YCoCg data comprise of int origmax4, ColorRanges ranges 
		transform->data_size = sizeof(int) + sizeof(ColorRanges);
		transform->data = malloc(transform->data_size);
		ColorRanges *ranges = (ColorRanges *)(transform->data + sizeof(int));
		ranges->num_planes = transform->s->channels;
		return 1;
	}
	else if(transform->desc == "Permute"){
		//PermutePlanes data comprise of flag uint8_t subtract, uint8_t permutation[5]
		transform->data_size = 6*sizeof(uint8_t) + sizeof(ColorRanges);
		transform->data = malloc(transform->data_size);
		uint8_t *subtract = (uint8_t *)transform->data;
		uint8_t *permutation = (uint8_t *)(transform->data + sizeof(uint8_t));
		ColorRanges *ranges = (ColorRanges *)(transform->data  + 6*sizeof(uint8_t));
		ranges->num_planes = transform->s->channels;
		*subtract = ff_flif16_rac_read_nz_int(transform->s->rc, 0, 1);
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
	return 0;
}

uint8_t transform_init(Transform *transform, ColorRanges *srcRanges){
	if(transform->desc == "YCoCg"){
		if(srcRanges->num_planes < 3)  return 0;
		if(srcRanges->min[0] == srcRanges->max[0] || srcRanges->min[1] == srcRanges->max[1] || srcRanges->min[2] == srcRanges->max[2])    return 0;
		if(srcRanges->min[0] < 1 || srcRanges->min[1] < 1 || srcRanges->min[2] < 1)  return 0;
		int *origmax4 = (int *)transform->data;
		*origmax4 = max(srcRanges->max[0], srcRanges->max[1], srcRanges->max[2])/4 -1;
		ColorRanges *ranges = (ColorRanges *)(transform->data + sizeof(int));
		int p;
		for(p = 0; p < ranges->num_planes; p++){
			ranges->max[p] = srcRanges->max[p];
			ranges->min[p] = srcRanges->min[p];
		}
		return 1;
	}
	else if(transform->desc == "Permute"){
		if(srcRanges->num_planes < 3)  return 0;
		if(srcRanges->min[0] < 1 || srcRanges->min[1] < 1 || srcRanges->min[2] < 1)  return 0;
		ColorRanges *ranges = (ColorRanges *)(transform->data + sizeof(int));
		int p;
		for(p = 0; p < ranges->num_planes; p++){
			ranges->max[p] = srcRanges->max[p];
			ranges->min[p] = srcRanges->min[p];
		}
		return 1;
	}
}

uint8_t transform_forward(Transform *transform, interimPixelData* pixelData){
    if(transform->desc == "YCoCg"){
		int r, c;
		ColorVal R,G,B,Y,Co,Cg;

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
		
		int *origmax4 = (int *)transform->data;
		ColorRanges *ranges = (ColorRanges *)(transform->data + sizeof(int));
		//Will see if ranges need to be stored separately in transform struct only or not.
		int p;
		for(p = 0; p < ranges->num_planes; p++){
			ranges->max[p] = max_range_YCoCg(p, *origmax4);
			ranges->min[p] = min_range_YCoCg(p, *origmax4);
		}

		//I don't know yet what to do with this flag in forward and reverse transforms.
		//transform->done = 1;
		return 1;
	}
	else if(transform->desc == "Permute"){
		//PENDING WORK
		//Actual forward transform is yet to be written
		uint8_t *subtract = (uint8_t *)transform->data;
		uint8_t *permutation = (uint8_t *)(transform->data + sizeof(uint8_t));
		ColorRanges *ranges = (ColorRanges *)(transform->data  + 6*sizeof(uint8_t));
		ranges->num_planes = transform->s->channels;

		if((*subtract)){
			if(ranges->num_planes > 3){
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
	return 0;
}

uint8_t transform_reverse(Transform* transform, interimPixelData *pixelData){
    int r, c;
    ColorVal R,G,B,Y,Co,Cg;

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

