#include "noise.cl"

#define BUMP 1260

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

	float3 npt = (float3)(fcoords.xy, 0);
	npt *= scale;

	float baseMarble = marble(npt, marbleTurb, 2.0, marbleDepth);
	float3 result = mix(baseColor0, baseColor1, 1.0 - baseMarble);
	float heightResult = 0.5 * baseMarble;

	float deepMarble = 1.0 - marble(npt, marbleTurb, 8.0, marbleDepth);
	deepMarble *= deepMarble;
	deepMarble *= deepMarble;
	result = mix(result, darkColor, deepMarble);

	float noiseMask = ClassicNoise3(npt * 0.5) * 0.5 + 0.5;
	noiseMask *= noiseMask;
	float noiseVal = noiseMask * fabs(multifractal(npt * noiseScalePt, lacunarity, H, octaves, offset));

	result *= mix((float3)(1), baseColor2, 1.0 - noiseScaleColor * noiseVal);

	heightResult *= 1.0 - noiseScaleHeight * noiseVal;

	float4 color = (float4)(result,1);
	color = pow(color, 2.2);
	float4 height = (float4)(heightResult);
	
	write_imagef(outputImage, coords, color);
	write_imagef(heightImage, coords, height);
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

