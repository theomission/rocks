#include "noise.cl"

__kernel void generateRockTexture(__write_only image2d_t outputImage)
{
	int2 coords = (int2)(get_global_id(0), get_global_id(1));
	
	float4 color = (float4)(1,1,1,1);
	
	write_imagef(outputImage, coords, color);
}

