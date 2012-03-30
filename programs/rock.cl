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

	float2 fcoords = convert_float2(coords) / convert_float2(dims);
	fcoords.y = 1 - fcoords.y;

	float3 npt = (float3)(fcoords.xy, 0);
	npt *= scale;

	float baseMarble = marble(npt, marbleTurb, 2.0, marbleDepth);
	baseMarble *= baseMarble;
	float3 result = mix(baseColor0, baseColor1, 1.0 - baseMarble);
	float heightResult = 0.5 * baseMarble;

	float deepMarble = 1.0 - marble(npt, marbleTurb, 32.0, marbleDepth);
	deepMarble *= deepMarble;
	result = mix(result, darkColor, deepMarble);

	float noiseMask = ClassicNoise3(npt * 0.5) * 0.5 + 0.5;
	noiseMask *= noiseMask;
	float noiseVal = noiseMask * fabs(multifractal(npt * noiseScalePt, lacunarity, H, octaves, offset));

	result *= mix((float3)(1), baseColor2, 1.0 - noiseScaleColor * noiseVal);

	heightResult *= 1.0 - noiseScaleHeight * noiseVal;

	float4 color = (float4)(result,1);
	float4 height = (float4)(heightResult);
	
	write_imagef(outputImage, coords, color);
	write_imagef(heightImage, coords, height);
}

