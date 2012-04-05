#include "noise.cl"

#define BUMP 1270

//sampler_t voronoiSampler = CLK_NORMALIZED_COORDS_TRUE | CLK_ADDRESS_CLAMP | CLK_FILTER_LINEAR;
__kernel void generateRockTexture(
	__write_only image2d_t outputImage,
	__write_only image2d_t heightImage,
	int marbleDepth,
	float marbleTurb,
	float3 baseColor0,
	float3 baseColor1,
	float3 baseColor2,
	float3 darkColor,
	float scale,
	float noiseScaleColor,
	float noiseScaleHeight,
	float noiseScalePt,
	float lacunarity,
	float H,
	int octaves,
	float offset
)
{
	int2 coords = (int2)(get_global_id(0), get_global_id(1));
	int2 dims = (int2)(get_global_size(0), get_global_size(1));

	float2 fcoords = convert_float2(coords) / convert_float2(dims - (int2)(1));
	fcoords.y = 1 - fcoords.y;

	float3 pt = (float3)(fcoords.xy, 0);
	float3 npt = pt * scale;

	float3 result = (float3)(0);
	float heightResult = 0;

	float baseMarble = marble(npt, marbleTurb, 3.0, marbleDepth);
	float deepMarble = marble(npt, marbleTurb, 4.0, marbleDepth);
	deepMarble = pow(1.0 - deepMarble, 32);

	float noiseMask = ClassicNoise3(npt*0.5)*0.5 + 0.5;
	noiseMask *= noiseMask;
	float noiseVal = noiseMask * multifractal(npt * noiseScalePt, lacunarity, H, octaves, offset);

	result = mix(baseColor0, baseColor1, clamp(1.0 - baseMarble, 0.0, 1.0));
	result = mix(result, baseColor2, clamp(noiseScaleColor * noiseVal, 0.0, 1.0));
	result = mix(result, darkColor, clamp(deepMarble, 0.0, 1.0));
	heightResult = 0.5 * baseMarble;
	heightResult = mix(heightResult, 1, clamp(noiseScaleHeight * noiseVal, 0.0, 1.0));
	heightResult = mix(heightResult, 0, clamp(deepMarble, 0.0, 1.0));

	float4 color = (float4)(result,1);
	color = pow(color, 2.2);
	float4 height = (float4)(heightResult);
	
	write_imagef(outputImage, coords, color);
	write_imagef(heightImage, coords, height);
}

__kernel void generateVoronoiTexture(
	__write_only image2d_t outputImage,
	unsigned int numPoints,
	__read_only __global float* inputPoints)
{
	int2 coords = (int2)(get_global_id(0), get_global_id(1));
	int2 dims = (int2)(get_global_size(0), get_global_size(1));
	float2 fcoords = convert_float2(coords) / convert_float2(dims - (int2)(1));
	fcoords.y = 1 - fcoords.y;

	float2 fdims = convert_float2(dims);
	const float invMaxLen = rsqrt(2.0);
	float bestVal = 1.0;
	for(unsigned int i = 0, offset = 0; i < numPoints; ++i, offset+=2)
	{
		float2 pt = (float2)(inputPoints[offset], inputPoints[offset+1]);
		float2 diff = pt - fcoords;
		float val = 10 * invMaxLen * sqrt(dot(diff,diff));
		if(val < bestVal) {
			bestVal = val;
		}
	}
	
	write_imagef(outputImage, coords, (float4)(bestVal));
}

__kernel void generateRockDensity(
	__write_only __global float *outDensity,
	float radius,
	float3 noiseScale,
	float H,
	float lacunarity,
	float octaves,
	float zCoord
	)
{
	int2 coords = (int2)(get_global_id(0), get_global_id(1));
	int2 dims = (int2)(get_global_size(0), get_global_size(1));
	
	float3 pt = (float3)(convert_float2(coords) / convert_float2(dims - (int2)(1)), zCoord);
	const float3 center = (float3)(0.5,0.5,0.5);
	float3 diff = pt - center;
	float len = length(diff);

	len += 0.03 * fbmNoise3(pt * noiseScale, H, lacunarity, octaves);

	int index = coords.x + coords.y * dims.x;
	float density = len - radius;//smoothstep(0, 0.1, radius - len);
	outDensity[index] = density;
}

