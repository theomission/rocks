// -*- C -*-

#ifndef INCLUDED_OPENCL_NOISE
#define INCLUDED_OPENCL_NOISE

// OpenCL perlin noise

#define TABLE_SIZE 256

__constant int g_permute[2 * TABLE_SIZE] = { 
5, 25, 124, 19, 16, 97, 76, 186, 183, 254, 37, 53, 87, 122, 66, 242, 8, 151,
1, 2, 111, 214, 208, 142, 74, 201, 82, 103, 245, 136, 96, 42, 50, 38, 212, 68,
32, 118, 59, 81, 43, 243, 190, 156, 199, 248, 147, 104, 213, 84, 4, 160, 24,
191, 115, 134, 145, 29, 113, 169, 91, 157, 21, 130, 116, 179, 210, 231, 83,
200, 64, 131, 128, 107, 95, 247, 71, 123, 226, 153, 48, 92, 13, 0, 78, 168,
221, 162, 35, 14, 94, 112, 233, 144, 109, 204, 100, 18, 77, 88, 52, 141, 165,
137, 227, 40, 99, 175, 203, 209, 72, 90, 63, 163, 235, 117, 47, 33, 44, 85,
119, 184, 150, 140, 189, 11, 75, 194, 7, 132, 121, 102, 174, 148, 39, 146, 195,
255, 143, 185, 170, 20, 239, 238, 225, 120, 79, 182, 192, 22, 207, 197, 9, 70,
196, 125, 101, 51, 135, 89, 45, 6, 252, 65, 187, 36, 172, 23, 230, 167, 127,
80, 3, 138, 246, 206, 240, 60, 234, 155, 244, 15, 161, 58, 10, 249, 180, 193,
34, 69, 251, 105, 158, 219, 108, 61, 27, 56, 31, 98, 152, 73, 205, 55, 67, 30,
188, 224, 215, 229, 17, 232, 159, 133, 54, 46, 202, 178, 93, 237, 149, 236,
171, 110, 181, 126, 176, 218, 12, 217, 114, 222, 241, 166, 216, 139, 62, 154,
223, 26, 220, 173, 253, 49, 198, 129, 164, 28, 211, 228, 86, 250, 41, 106, 57,
177, 
5, 25, 124, 19, 16, 97, 76, 186, 183, 254, 37, 53, 87, 122, 66, 242, 8, 151, 1,
2, 111, 214, 208, 142, 74, 201, 82, 103, 245, 136, 96, 42, 50, 38, 212, 68, 32,
118, 59, 81, 43, 243, 190, 156, 199, 248, 147, 104, 213, 84, 4, 160, 24, 191,
115, 134, 145, 29, 113, 169, 91, 157, 21, 130, 116, 179, 210, 231, 83, 200, 64,
131, 128, 107, 95, 247, 71, 123, 226, 153, 48, 92, 13, 0, 78, 168, 221, 162,
35, 14, 94, 112, 233, 144, 109, 204, 100, 18, 77, 88, 52, 141, 165, 137, 227,
40, 99, 175, 203, 209, 72, 90, 63, 163, 235, 117, 47, 33, 44, 85, 119, 184,
150, 140, 189, 11, 75, 194, 7, 132, 121, 102, 174, 148, 39, 146, 195, 255, 143,
185, 170, 20, 239, 238, 225, 120, 79, 182, 192, 22, 207, 197, 9, 70, 196, 125,
101, 51, 135, 89, 45, 6, 252, 65, 187, 36, 172, 23, 230, 167, 127, 80, 3, 138,
246, 206, 240, 60, 234, 155, 244, 15, 161, 58, 10, 249, 180, 193, 34, 69, 251,
105, 158, 219, 108, 61, 27, 56, 31, 98, 152, 73, 205, 55, 67, 30, 188, 224,
215, 229, 17, 232, 159, 133, 54, 46, 202, 178, 93, 237, 149, 236, 171, 110,
181, 126, 176, 218, 12, 217, 114, 222, 241, 166, 216, 139, 62, 154, 223, 26,
220, 173, 253, 49, 198, 129, 164, 28, 211, 228, 86, 250, 41, 106, 57, 177
};

#define GRAD_MASK 15

__constant float3 g_grad[16] = {
	(float3)(1.0, 1.0, 0.0),
	(float3)(-1.0, 1.0, 0.0),
	(float3)(1.0, -1.0, 0.0),
	(float3)(-1.0, -1.0, 0.0),
	
	(float3)(1.0, 0.0, 1.0),
	(float3)(-1.0, 0.0, 1.0),
	(float3)(1.0, 0.0, -1.0),
	(float3)(-1.0, 0.0, -1.0),
	
	(float3)(0.0, 1.0, 1.0),
	(float3)(0.0, -1.0, 1.0),
	(float3)(0.0, 1.0, -1.0),
	(float3)(0.0, -1.0, -1.0),
	
	(float3)(1.0, 1.0, 0.0),
	(float3)(-1.0, 1.0, 0.0),
	(float3)(0.0, -1.0, 1.0),
	(float3)(0.0, -1.0, -1.0),
};

int Fold3(int3 pt)
{
	return g_permute[g_permute[g_permute[pt.x] + pt.y] + pt.z];
}

float3 Grad(int perm)
{
	float3 grad = g_grad[perm & GRAD_MASK];
	float scale = rsqrt(dot(grad,grad));
	return grad * scale;
}

float3 spline_c2(float3 t)
{
	float3 t2 = t*t;
	float3 t3 = t2*t;
	float3 t4 = t2 * t2;
	float3 t5 = t2 * t3;
	return 6.f * t5 - 15.f * t4 + 10 * t3;
}

float ClassicNoise3(float3 pt)
{
	float3 ptLo;
	float3 pos = fract(pt, &ptLo);
	int3 iptLo = convert_int3(ptLo) % TABLE_SIZE;
	int3 iptHi = iptLo + (int3)(1,1,1);

	float3 npos = pos - (float3)(1);

	int pt000 = Fold3(iptLo);
	int pt100 = Fold3((int3)(iptHi.x, iptLo.y, iptLo.z));
	int pt010 = Fold3((int3)(iptLo.x, iptHi.y, iptLo.z));
	int pt110 = Fold3((int3)(iptHi.x, iptHi.y, iptLo.z));
	int pt001 = Fold3((int3)(iptLo.x, iptLo.y, iptHi.z));
	int pt101 = Fold3((int3)(iptHi.x, iptLo.y, iptHi.z));
	int pt011 = Fold3((int3)(iptLo.x, iptHi.y, iptHi.z));
	int pt111 = Fold3(iptHi.xyz);

	float3 n000 = pos;
	float3 n100 = (float3)(npos.x, pos.y, pos.z);
	float3 n010 = (float3)(pos.x, npos.y, pos.z);
	float3 n110 = (float3)(npos.x, npos.y, pos.z);
	float3 n001 = (float3)(pos.x, pos.y, npos.z);
	float3 n101 = (float3)(npos.x, pos.y, npos.z);
	float3 n011 = (float3)(pos.x, npos.y, npos.z);
	float3 n111 = npos.xyz;

	float g000 = dot(n000, Grad(pt000));
	float g100 = dot(n100, Grad(pt100));
	float g010 = dot(n010, Grad(pt010));
	float g110 = dot(n110, Grad(pt110));
	float g001 = dot(n001, Grad(pt001));
	float g101 = dot(n101, Grad(pt101));
	float g011 = dot(n011, Grad(pt011));
	float g111 = dot(n111, Grad(pt111));

	float3 coords = spline_c2(pos);

	float dotx0 = mix(g000, g100, coords.x);
	float dotx1 = mix(g010, g110, coords.x);
	float dotx2 = mix(g001, g101, coords.x);
	float dotx3 = mix(g011, g111, coords.x);

	float doty0 = mix(dotx0, dotx1, coords.y);
	float doty1 = mix(dotx2, dotx3, coords.y);

	float dotz = mix(doty0, doty1, coords.z);
	return dotz;
}

float fbmNoise3(float3 pt, float h, float lacunarity, float octaves)
{
	int numOctaves = convert_int(octaves);
	float result = 0;
	for(int i = 0; i < numOctaves; ++i)
	{
		result += ClassicNoise3(pt) * pow(lacunarity, -h * i);
		pt *= lacunarity;
	}

	float remainder = octaves - numOctaves;
	if(remainder > 0)
	{
		result += remainder * ClassicNoise3(pt) * pow(lacunarity, -h * numOctaves);
	}
	return result;
}

float multifractal(float3 pt, float h, float lacunarity, int numOctaves, float offset)
{
	float result = 1;
	for(int i = 0; i < numOctaves; ++i)
	{
		result *= (ClassicNoise3(pt) + offset) * pow(lacunarity, -h * i);
		pt *= lacunarity;
	}

	return result;
	
}

float turbulence(float3 pt, int depth)
{
	float result = 0;
	float scale = 1.0;
	for(int i = 0; i < depth; ++i)
	{
		pt /= scale;
		result += (0.5 * ClassicNoise3(pt) + 0.5) * scale;
		scale *= 0.5;
	}
	return result;
}

float marble(float3 npt, float turbulenceMult, float power, float depth)
{
	float val = npt.y + npt.x + turbulenceMult * turbulence(npt, depth);
	val = 0.5 * sin(M_PI * val) + 0.5;
	val = 1.0 - pow(val,power);
	return val;
}


#endif

