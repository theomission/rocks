#include "noise.cl"

#define BUMP 1258

__kernel void generateRockTexture(__write_only image2d_t outputImage)
{
	int2 coords = (int2)(get_global_id(0), get_global_id(1));
	int2 dims = (int2)(get_global_size(0), get_global_size(1));

	float2 fcoords = convert_float2(coords) / convert_float2(dims);

	float3 npt = (float3)(fcoords.xy, 0);
	npt *= 5;

	float n = fbmNoise3(npt, 0.9, 1.9, 16) * 0.5 + 0.5;
	const float3 layer0Color = (float3)(0.7, 0.69, 0.65);
	float3 layer0Value = n > 0.5 ? layer0Color : (float3)(0);
	
	n = fbmNoise3(npt * 2, 0.3, 2, 10) * 0.5 + 0.5;
	const float3 layer1Color = (float3)(0.4, 0.69, 0.65);
	float3 layer1Value = n > 0.5 ? layer1Color : (float3)(0);

	float3 result = layer0Value + layer1Value;
	float4 color = (float4)(result,1);
	
	write_imagef(outputImage, coords, color);
}

