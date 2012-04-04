#include "common.hh"
#include "mesh.hh"
#include <algorithm>
#include <limits>
#include "render.hh"

////////////////////////////////////////////////////////////////////////////////
	TriSoup::TriSoup()
	: m_faces()
	  , m_vertices()
{
}

void TriSoup::Clear()
{
	m_faces.clear();
	m_vertices.clear();
}

int TriSoup::AddVertex(const vec3& pos)
{
	int idx = m_vertices.size();
	m_vertices.emplace_back(pos);
	return idx;
}

int TriSoup::AddFace(int v0, int v1, int v2)
{
	int idx = m_faces.size();
	m_faces.emplace_back(v0, v1, v2);
	return idx;
}

void TriSoup::DeleteFace(int index)
{
	int lastIdx = m_faces.size() - 1;
	if(index != lastIdx)
		std::swap(m_faces[lastIdx], m_faces[index]);
	m_faces.pop_back();
}

const vec3& TriSoup::GetVertexPos(int index) const
{
	return m_vertices[index].m_pos;
}

const vec3& TriSoup::GetVertexNormal(int index) const
{
	return m_vertices[index].m_normal;
}

void TriSoup::GetFace(int index, int (&indices)[3]) const
{
	const Face& face = m_faces[index];
	for(int i = 0; i < 3; ++i)
		indices[i] = face.m_vertices[i];
}

// Cache sorting
// See: http://home.comcast.net/~tom_forsyth/papers/fast_vert_cache_opt.html

////////////////////////////////////////////////////////////////////////////////
// Resorting part
// Finds a draw list using reindexing. Will put vertices in order of appearance
// int the draw list, remapping the indices as it goes. 
// The contents of the part will be overwritten.
void TriSoup::CacheSort(int lruSize)
{
	std::vector<int> faceOrder;
	ReindexTriangles(lruSize, faceOrder);
	RemapData(faceOrder);
}

////////////////////////////////////////////////////////////////////////////////
// LRU cache model for verts
namespace CacheSort
{
	////////////////////////////////////////////////////////////////////////////////        
	// Constnats
	static const float kInitialScore = 0.75f;
	static const float kLruScorePower = 1.5f; // exponential score, dampens small values a little faster than linear
	static const float kValencyScale = 2.f;
	static const float kValencyPower = -0.5f; // scales small numbers a lot

	////////////////////////////////////////////////////////////////////////////////
	// Helper structures

	////////////////////////////////////////////////////////////////////////////////        
	struct Vertex
	{
		Vertex() ;

		float m_score;
		mutable int m_cacheIndex;
		int m_valency;
		int m_numFaces;
		std::vector<int> m_clientFaces;

		void InitClientFaces(int size);
		void ComputeScore(int lruSize);
	};

	Vertex::Vertex() 
		: m_score(0.f)
		, m_cacheIndex(-1)
		, m_valency(0)
		, m_numFaces(0)
		, m_clientFaces()
	{
	}
	void Vertex::InitClientFaces(int size) 
	{
		m_clientFaces.resize(size);
		std::fill(m_clientFaces.begin(), m_clientFaces.end(), -1);
	}

	void Vertex::ComputeScore(int cacheSize)
	{
		float score = 0.f;
		if(m_cacheIndex >= 0)   
		{
			if(m_cacheIndex < 3) {
				score = kInitialScore;
			} else {
				float t = 1.f - (( m_cacheIndex - 3) / float(cacheSize - 3));
				score = powf(t, kLruScorePower);
			}
		} 

		float valencyBoost = kValencyScale * powf( (float)m_valency, kValencyPower );
		score += valencyBoost;

		m_score = score;
	}

	////////////////////////////////////////////////////////////////////////////////        
	struct Face
	{
		Face();

		static void RemoveConnections(int myIndex, std::vector<Vertex>& verts, std::vector<Face>& faces);

		int m_indices[3];
		bool m_drawn;
	};

	Face::Face()
		: m_indices{0,0,0}
		, m_drawn(false)
	{
	}

	void Face::RemoveConnections(int myIndex, std::vector<Vertex>& verts, std::vector<Face>& faces)
	{
		for(int i = 0; i < 3; ++i) 
		{
			int idxVert = faces[myIndex].m_indices[i];
			// remove the curFace from the client faces of each vertex touching this face
			for(int j = 0; j < verts[idxVert].m_valency; ++j)
			{
				int idxFace = verts[idxVert].m_clientFaces[j] ;
				if( idxFace == myIndex )
				{
					int last = verts[idxVert].m_valency - 1;
					verts[idxVert].m_clientFaces[j] = 
						verts[idxVert].m_clientFaces[last];
					verts[ idxVert ].m_valency = last;
					break;
				}
			}
			ASSERT(verts[idxVert].m_valency >= 0);
		}
	}
	////////////////////////////////////////////////////////////////////////////////        
	class VertexLRU
	{
		int m_maxCacheSize;
		std::vector<int> m_cache;
		std::vector<int> m_cacheOther;

		int FindVert(int vertId )
		{
			for(int i = 0; i < m_maxCacheSize; ++i) {
				if(m_cache[i] == vertId)
					return i;
			}
			return -1;
		}

		void AddVert(const std::vector<Vertex>& verts, int vertIdx, int faceVertIdx)
		{
			ASSERT(faceVertIdx >= 0 && faceVertIdx < 3);
			const Vertex* vert = &verts[vertIdx];
			if(vert->m_cacheIndex >= 0)
			{
				ASSERT(vert->m_cacheIndex < m_maxCacheSize);
				m_cache[vert->m_cacheIndex] = -1;
			}
			m_cacheOther[faceVertIdx] = vertIdx;
			vert->m_cacheIndex = faceVertIdx;       // it'll be one of the first 3 indices so assigning the
			// face vert index is reasonable.
		}

	public:
		VertexLRU(int size)
			: m_maxCacheSize(size)
			, m_cache(size, -1)
			, m_cacheOther(size, -1)
		{
		}
		void AddFace(Face* f, std::vector<Vertex>& verts) 
		{       
			ASSERT(!f->m_drawn);
			f->m_drawn = true;
			for(int i = 0; i < 3; ++i) {
				int vertIdx = f->m_indices[i];
				AddVert(verts, vertIdx, i);
			}

			int srcIdx = 0;
			int destIdx = 3;
			while( srcIdx < m_maxCacheSize && destIdx < m_maxCacheSize)
			{
				// A vert has been moved if it was added when already in the cache, so skip over those.
				if(m_cache[srcIdx] >= 0) {
					m_cacheOther[destIdx] = m_cache[srcIdx];
					verts[m_cacheOther[destIdx]].m_cacheIndex = destIdx;
					++destIdx;
				}
				++srcIdx;
			}

			// Let the verts that have been pushed out of the cache know that they're no longer there.
			for(int j = srcIdx; j < m_maxCacheSize; ++j) {
				if(m_cache[j] >= 0) {
					verts[m_cache[j]].m_cacheIndex = -1;
				}
			}

			m_cache.swap(m_cacheOther);
		}

		void Truncate(int size, std::vector<Vertex>& verts) 
		{
			for(int i = size; i < m_maxCacheSize; ++i) {
				if(m_cache[i] >= 0) {
					verts[m_cache[i]].m_cacheIndex = -1;
				}
				m_cache[i] = -1;
			}
		}

		const int* GetVertexCache() const { return &m_cache[0]; }
		int GetCacheSize() const { return m_maxCacheSize; }
	};
	////////////////////////////////////////////////////////////////////////////////
	// Sorting code

	// Return the best face, disregarding the LRU. Used for initialization.
	int FindBestFaceGlobal(std::vector<Vertex>& verts, std::vector<Face>& faces)
	{
		for(int i = 0, c = verts.size(); i < c; ++i)
			verts[i].ComputeScore(0);

		int bestFaceIdx = -1;
		float bestScore = 0.f;

		for(int i = 0, c = faces.size(); i < c; ++i)
		{
			// If this is the first time, no faces will have been drawn, but in case this funtion
			// gets used for something else, later...
			if(!faces[i].m_drawn)
			{
				float faceScore = 
					verts[faces[i].m_indices[0]].m_score +
					verts[faces[i].m_indices[1]].m_score +
					verts[faces[i].m_indices[2]].m_score ;

				if(faceScore > bestScore) {
					bestFaceIdx = i;
					bestScore = faceScore;
				}
			}

		}

		return bestFaceIdx;
	}
	void InitValence(std::vector<Vertex>& verts, std::vector<Face>& faces)
	{
		for(int i = 0, c = verts.size(); i < c; ++i) 
			verts[i].m_numFaces = 0;

		for(int i = 0, c = faces.size(); i < c; ++i)
		{
			for(int j = 0; j < 3; ++j)
			{
				int vertIdx = faces[i].m_indices[j] ; 
				if(vertIdx >= (int)verts.size()) {
					std::cerr << "vert idx " << vertIdx << " is out of range (# verts " <<
						verts.size() << ")" << std::endl;
					exit(1);
				}
				Vertex& vert = verts[vertIdx];
				++vert.m_numFaces;
			}
		}

		for(int i = 0, c = verts.size(); i < c; ++i)
		{
			Vertex& vert = verts[i];
			vert.m_valency = 0;
			vert.InitClientFaces( vert.m_numFaces );
		}

		for(int i = 0, c = faces.size(); i < c; ++i)
		{
			for(int j = 0; j < 3; ++j)
			{
				Vertex& vert = verts[ faces[i].m_indices[j] ];
				vert.m_clientFaces[ vert.m_valency++ ] = i;
			}
		}
	}

}

void TriSoup::ReindexTriangles(int lruSize, std::vector<int>& outFaceOrder)
{
	////////////////////////////////////////        
	// Init vert and face arrays.
	const int numVerts = NumVertices();
	const int numTris = NumFaces();

	std::vector<CacheSort::Vertex> verts(numVerts);
	std::vector<CacheSort::Face> faces(numTris);

	for(int i = 0, c = NumFaces(); i < c; ++i)
	{
		faces[i].m_indices[0] = m_faces[i].m_vertices[0];
		faces[i].m_indices[1] = m_faces[i].m_vertices[1];
		faces[i].m_indices[2] = m_faces[i].m_vertices[2];
	}

	CacheSort::InitValence(verts, faces);

	outFaceOrder = std::vector<int>(numTris, -1);

	////////////////////////////////////////        
	// Finished init. now start the main loop
	CacheSort::VertexLRU lru(lruSize+3); // 3 extra to hold up to 3 verts which get pushed off.
	const int *cache = lru.GetVertexCache();
	int cacheSize = lru.GetCacheSize();

	int idxDrawList = 0;

	int curFace = CacheSort::FindBestFaceGlobal(verts, faces);
	while(curFace >= 0) 
	{
		ASSERT( idxDrawList < numTris );

		// Add this face to the draw list.
		outFaceOrder[idxDrawList++] = curFace;
		lru.AddFace(&faces[curFace], verts);

		// decrement current valency so we don't keep computing scores for already drawn faces
		CacheSort::Face::RemoveConnections(curFace, verts, faces);

		// Recompute scores of all verts in the LRU
		for(int i = 0; i < cacheSize && cache[i] >= 0; ++i)
		{
			int idxVert = cache[i];
			verts[idxVert].ComputeScore(cacheSize);
		}

		int bestFace = -1;
		float bestFaceScore = 0.f;

		// Recompute scores of all faces whose scores would change due to changes in the LRU
		for(int i = 0; i < cacheSize && cache[i] >= 0; ++i)
		{
			int idxVert = cache[i];
			for(int j = 0; j < verts[idxVert].m_valency; ++j) 
			{
				int idxFace = verts[idxVert].m_clientFaces[j];
				float faceScore = 
					verts[ faces[idxFace].m_indices[0] ].m_score +
					verts[ faces[idxFace].m_indices[1] ].m_score +
					verts[ faces[idxFace].m_indices[2] ].m_score ;

				if (faceScore > bestFaceScore) {
					bestFace = idxFace;
					bestFaceScore = faceScore;
				}
			}
		}
		// Truncate the extra 3 verts. This just means if the next face includes one of the removed verts, it will be added instead of
		// swapped, which will push more verts off. Also it means those 3 verts will NOT be included in scoring next iteration.
		lru.Truncate(lruSize, verts);

		// If for some reason the valency information doesn't link adjacent faces, then just pick the best global face again.
		if( bestFace < 0 ) 
			curFace = CacheSort::FindBestFaceGlobal(verts, faces);
		else
			curFace = bestFace;
	}
}

void TriSoup::RemapData( const std::vector<int>& faceOrder )
{
	ASSERT(faceOrder.size() == m_faces.size());
	// use the new cache aware index buffer to reorder the vertex buffer data. Create a map 
	// from idxOld -> idxNew.
	const int numVerts = NumVertices();

	std::vector<int> vertRemap(numVerts, -1);
	std::vector<Vertex> newVerts(NumVertices());
	std::vector<Face> newFaces(NumFaces());

	int curVert = 0;
	for(int i = 0, c = faceOrder.size(); i < c; ++i)
	{
		// put the face in the correct order
		ASSERT(faceOrder[i] < NumFaces());
		newFaces[i] = m_faces[faceOrder[i]];

		// remap the vertices in the current face
		for(int j = 0; j < 3; ++j)
		{
			int v0 = newFaces[i].m_vertices[j];
			if(vertRemap[v0] == -1)
			{
				newVerts[curVert] = m_vertices[v0];
				vertRemap[v0] = curVert;
				++curVert;
			}
			newFaces[i].m_vertices[j] = vertRemap[v0];
		}
	}

	ASSERT(curVert == (int)vertRemap.size());

	m_faces.swap(newFaces);
	m_vertices.swap(newVerts);
}

void TriSoup::Merge(const TriSoup* other)
{
	const int prevNumVerts = NumVertices();

	for(int i = 0, c = other->NumVertices(); i < c; ++i)
		AddVertex(other->m_vertices[i].m_pos);

	for(int i = 0, c = other->NumFaces(); i < c; ++i)
	{
		int indices[3];
		other->GetFace(i, indices);
		for(int j = 0; j < 3; ++j)
			indices[j] += prevNumVerts;
		AddFace(indices[0], indices[1], indices[2]);
	}
}
	
void TriSoup::ComputeNormals()
{
	for(auto& vtx: m_vertices)
		vtx.m_normal = vec3(0);

	for(const auto& face: m_faces)
	{
		vec3 v0 = m_vertices[face.m_vertices[0]].m_pos;
		vec3 v1 = m_vertices[face.m_vertices[1]].m_pos;
		vec3 v2 = m_vertices[face.m_vertices[2]].m_pos;

		vec3 n = (Cross(v1-v0,v2-v0));

		m_vertices[face.m_vertices[0]].m_normal += n;
		m_vertices[face.m_vertices[1]].m_normal += n;
		m_vertices[face.m_vertices[2]].m_normal += n;
	}

	for(auto& vtx: m_vertices)
		vtx.m_normal.Normalize();
}

template<typename IndexType>
std::shared_ptr<Geom> TriSoup::CreateGeom(const TriSoup& soup)
{
	std::vector<IndexType> indices(3 * soup.NumFaces());
	std::vector<float> vertexData(6 * soup.NumVertices());

	const int vtxStride = 6 * sizeof(float);
	unsigned int offset = 0;
	const unsigned int numVertices = 
		Min<unsigned int>(soup.m_vertices.size(), std::numeric_limits<IndexType>::max());
	for(unsigned int i = 0; i < numVertices; ++i)
	{
		const auto& vtx = soup.m_vertices[i];
		vertexData[offset++] = vtx.m_pos.x;
		vertexData[offset++] = vtx.m_pos.y;
		vertexData[offset++] = vtx.m_pos.z;
		vertexData[offset++] = vtx.m_normal.x;
		vertexData[offset++] = vtx.m_normal.y;
		vertexData[offset++] = vtx.m_normal.z;
	}

	offset = 0;
	for(const auto& face: soup.m_faces)
	{
		if(std::any_of(face.m_vertices, face.m_vertices+3, 
			[](int val) { return (unsigned int)val > std::numeric_limits<IndexType>::max(); }))
			continue;
		
		indices[offset++] = face.m_vertices[0];
		indices[offset++] = face.m_vertices[1];
		indices[offset++] = face.m_vertices[2];
	}
	const unsigned int numIndices = offset;

	return std::make_shared<Geom>(
		numVertices, &vertexData[0],
		numIndices, &indices[0],
		vtxStride, GL_TRIANGLES,
		std::vector<GeomBindPair>{
			{GEOM_Pos, 3, 0},
			{GEOM_Normal, 3, 3 * sizeof(float)},
		});
}

std::shared_ptr<Geom> TriSoup::CreateGeom() const
{
	if(NumVertices() > std::numeric_limits<unsigned short>::max())
		return CreateGeom<unsigned int>(*this);
	else
		return CreateGeom<unsigned short>(*this);
}

////////////////////////////////////////////////////////////////////////////////
PointGrid::PointGrid(const AABB& bounds, float bucketDim)
	: m_bounds(bounds)
	, m_cellsx( ceilf(bounds.Width() / bucketDim) )
	, m_cellsy( ceilf(bounds.Height() / bucketDim) )
	, m_cellsz( ceilf(bounds.Depth() / bucketDim) )
	, m_grid( m_cellsx * m_cellsy * m_cellsz )
	, m_dim(bucketDim)
{
}

int PointGrid::Find(const vec3& pt) const
{
	float invBucketDim = 1.f/m_dim;
	vec3 lo = pt - vec3(kEpsilon) - m_bounds.m_min;
	vec3 hi = pt + vec3(kEpsilon) - m_bounds.m_min;
	lo *= invBucketDim;
	hi *= invBucketDim;
	int startZ = int(lo.z), endZ = int(hi.z);
	int startY = int(lo.y), endY = int(hi.y);
	int startX = int(lo.x), endX = int(hi.x);
	startZ = Clamp(startZ, 0, m_cellsz - 1);
	endZ = Clamp(endZ, 0, m_cellsz - 1);
	startY = Clamp(startY, 0, m_cellsy - 1);
	endY = Clamp(endY, 0, m_cellsy - 1);
	startX = Clamp(startX, 0, m_cellsx - 1);
	endX = Clamp(endX, 0, m_cellsx - 1);

	const int pitch = m_cellsx;
	const int slicePitch = m_cellsx * m_cellsy;
	for(int z = startZ; z <= endZ; ++z)
	{
		for(int y = startY; y <= endY; ++y)
		{
			for(int x = startX; x <= endX; ++x)
			{
				const VecListType* list = m_grid[x + y * pitch + z * slicePitch].get();
				if(list)
				{
					for(const auto& pair: *list)
					{
						vec3 diff = pair.vec - pt;
						if(LengthSq(diff) < kEpsilonSq) {
							return pair.index;
						}
					}
				}
			}
		}
	}

	return -1;
}

void PointGrid::Add(int index, const vec3& pt)
{
	vec3 gridCell = (pt - m_bounds.m_min) / m_dim;
	int x = int(gridCell.x),
		y = int(gridCell.y),
		z = int(gridCell.z);

	z = Clamp(z, 0, m_cellsz - 1);
	y = Clamp(y, 0, m_cellsy - 1);
	x = Clamp(x, 0, m_cellsx - 1);
	const int pitch = m_cellsx;
	const int slicePitch = m_cellsx * m_cellsy;
	unsigned int offset = x + y * pitch + z * slicePitch;
	if(!m_grid[offset])
		m_grid[offset] = std::make_shared<VecListType>();

	VecListType *list = m_grid[offset].get();
	list->emplace_back(pt, index);
}

////////////////////////////////////////////////////////////////////////////////
int UniqueAddVertex(TriSoup* mesh, PointGrid& grid, const vec3& pt)
{
	int index = grid.Find(pt);
	if(index < 0) {
		index = mesh->AddVertex(pt);
		grid.Add(index, pt);
	}
	return index;
}

