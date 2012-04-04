#include "surfcon.hh"
#include "mesh.hh"

static const unsigned int g_edgeTable[16] =
{
	0x00, // 0000
	0x0D, // 0001
	0x13, // 0010
	0x1E, // 0011
	0x26, // 0100
	0x2B, // 0101
	0x35, // 0110
	0x38, // 0111
	0x38, // 1000
	0x35, // 1001
	0x2B, // 1010
	0x26, // 1011
	0x1E, // 1100
	0x13, // 1101
	0x0D, // 1110
	0x00  // 1111
};

static const int g_triTable[16][6] =
{
	{ -1, -1, -1, -1, -1, -1 }, // 0000
	{ 0, 3, 2, -1, -1, -1 },        // 0001
	{ 0, 1, 4, -1, -1, -1 },        // 0010
	{ 1, 4, 3, 1, 3, 2 },           // 0011 
	{ 1, 2, 5, -1, -1, -1 },        // 0100
	{ 1, 3, 5, 1, 0, 3 },           // 0101
	{ 2, 5, 4, 2, 4, 0 },           // 0110
	{ 3, 5, 4, -1, -1, -1 },        // 0111
	{ 3, 4, 5, -1, -1, -1 },        // 1000
	{ 2, 4, 5, 2, 0, 4 },           // 1001
	{ 1, 5, 3, 1, 3, 0 },           // 1010
	{ 1, 5, 2, -1, -1, -1 },        // 1011
	{ 1, 3, 4, 1, 2, 3 },           // 1100 
	{ 0, 4, 1, -1, -1, -1 },        // 1101
	{ 0, 2, 3, -1, -1, -1 },        // 1110
	{ -1, -1, -1, -1, -1, -1 }, // 1111
};

static inline vec3 InterpPoints(const vec3& v0, const vec3& v1, float d0, float d1)
{
	float t = d0 / (d0 - d1);
	t = Clamp(t, 0.f, 1.f);
	return (1.f - t) * v0 + t * v1;
}

static inline unsigned int MakeTableIndex(float (&distances)[4])
{
	unsigned int index = 0;
	index |= distances[0] < 0 ? 1 : 0;
	index |= distances[1] < 0 ? 2 : 0;
	index |= distances[2] < 0 ? 4 : 0;
	index |= distances[3] < 0 ? 8 : 0;
	return index;
}

static void surfcon_CreateFaces(TriSoup* result,
	PointGrid& grid, 
	const float *samples, const vec3* points);

std::shared_ptr<TriSoup> surfcon_CreateMeshFromDensityField(
	float isolevel,
	const float* densityField, 
	unsigned int width, unsigned int height, unsigned int depth)
{
	std::shared_ptr<TriSoup> result = std::make_shared<TriSoup>();
	TriSoup* mesh = result.get();
	unsigned int slicePitch = width * height;
	unsigned int zOffset = 0;
	const float smallestSide = Min(Min(width,height),depth);
	const float inc = 2.0 / smallestSide;
	const vec3 sideScale = vec3(width,height,depth) / smallestSide;
	PointGrid grid(AABB(-sideScale, sideScale), 1.0f/64);
	const vec3 startPt = -sideScale;
	for(int z = 0, zMax = depth - 1; z < zMax; ++z)
	{
		unsigned int yOffset = 0;
		for(int y = 0, yMax = depth - 1; y < yMax; ++y)
		{
			for(int x = 0, xMax = depth - 1; x < xMax; ++x)
			{
				const unsigned int off = x + yOffset + zOffset;
				const unsigned int off2 = off + slicePitch;
				const unsigned int offsets[8] = {
					off,
					off + 1,			// + (1,0,0)
					off + 1 + width, 	// + (1,1,0)
					off + width,		// + (0,1,0)
					off2,				// + (0,0,1)
					off2 + 1,			// + (1,0,1)
					off2 + 1 + width,	// + (1,1,1)
					off2 + width,		// + (0,1,1)
				};

				const vec3 pt = startPt + inc * vec3(x,y,z);
				const vec3 points[8] = {
					pt,
					pt + vec3(inc,0,0),
					pt + vec3(inc,inc,0),
					pt + vec3(0,inc,0),
					pt + vec3(0,0,inc),
					pt + vec3(inc,0,inc),
					pt + vec3(inc,inc,inc),
					pt + vec3(0,inc,inc),
				};

				float samples[8];
				for(int i = 0; i < 8; ++i) 
					samples[i] = densityField[offsets[i]] - isolevel;
			
				surfcon_CreateFaces(mesh, grid, samples, points);

			}
			yOffset += width;
		}
		zOffset += slicePitch;
	}

	std::cout << "mesh has " << result->NumVertices() << " verts and " <<
		result->NumFaces() << " faces. " <<std::endl;

	return result;
}

static void surfcon_CreateTetrahedronTris(TriSoup* result,
	PointGrid& grid, 
	const float* samples, const vec3* points, int v0, int v1, int v2, int v3)
{
	float localSamples[4] = {
		samples[v0], samples[v1], samples[v2], samples[v3] };
	unsigned int tetIndex = MakeTableIndex(localSamples);
	int edges = g_edgeTable[tetIndex];
	
	int edgeVerts[6] = {-1,-1,-1,-1,-1,-1};
	if(edges & 1)
		edgeVerts[0] = UniqueAddVertex(result, grid, 
			InterpPoints(points[v0], points[v1], samples[v0], samples[v1]));
	if(edges & 2)
		edgeVerts[1] = UniqueAddVertex(result, grid,
			InterpPoints(points[v1], points[v2], samples[v1], samples[v2]));
	if(edges & 4)
		edgeVerts[2] = UniqueAddVertex(result, grid,
			InterpPoints(points[v2], points[v0], samples[v2], samples[v0]));
	if(edges & 8)
		edgeVerts[3] = UniqueAddVertex(result, grid,
			InterpPoints(points[v0], points[v3], samples[v0], samples[v3]));
	if(edges & 16)
		edgeVerts[4] = UniqueAddVertex(result, grid,
			InterpPoints(points[v1], points[v3], samples[v1], samples[v3]));
	if(edges & 32)
		edgeVerts[5] = UniqueAddVertex(result, grid,
			InterpPoints(points[v2], points[v3], samples[v2], samples[v3]));

	const int* triTable = g_triTable[tetIndex];
	for(int i = 0; i < 6 && triTable[i] >= 0; i += 3)
	{
		result->AddFace(edgeVerts[triTable[i]],
			edgeVerts[triTable[i+1]],
			edgeVerts[triTable[i+2]]);
	}
}

static void surfcon_CreateFaces(TriSoup* result,
	PointGrid& grid,
	const float *samples, const vec3* points)
{
	surfcon_CreateTetrahedronTris(result, grid, samples, points, 0, 3, 1, 5);
	surfcon_CreateTetrahedronTris(result, grid, samples, points, 1, 3, 2, 5);
	surfcon_CreateTetrahedronTris(result, grid, samples, points, 4, 5, 7, 3);
	surfcon_CreateTetrahedronTris(result, grid, samples, points, 5, 6, 7, 3);
	surfcon_CreateTetrahedronTris(result, grid, samples, points, 0, 5, 4, 3);
	surfcon_CreateTetrahedronTris(result, grid, samples, points, 5, 2, 6, 3);
}

