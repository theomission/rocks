#pragma once

#include <vector>
#include <memory>
#include "vec.hh"
#include "commonmath.hh"

class Geom;

////////////////////////////////////////////////////////////////////////////////
// TriSoup
// triangle mesh with no topology info or guarantees. Just a bunch of tris.
class TriSoup
{
public:
	////////////////////////////////////////////////////////////////////////////////        
	TriSoup();

	// Creation functions
	int AddVertex(const vec3& pos);
	int AddFace(int v0, int v1, int v2);

	// Modification functions
	void Clear();
	void DeleteFace(int index);

	// Accessor functions
	int NumFaces() const { return m_faces.size(); }
	int NumVertices() const { return m_vertices.size(); }

	const vec3& GetVertexPos(int index) const;
	const vec3& GetVertexNormal(int index) const;
	void GetFace(int index, int (&indices)[3]) const;

	void CacheSort(int lruCacheSize);
	void Merge(const TriSoup* other);

	void ComputeNormals();
	std::shared_ptr<Geom> CreateGeom() const;
private:
	template<typename T> static std::shared_ptr<Geom> CreateGeom(const TriSoup&);

	////////////////////////////////////////////////////////////////////////////////        
	struct Face {
		Face() {}
		Face(int v0, int v1, int v2) : m_vertices{v0,v1,v2} {}
		int m_vertices[3];
	};
	struct Vertex {
		Vertex() {}
		Vertex(const vec3& pos) : m_pos(pos), m_normal() {}
		vec3 m_pos;
		vec3 m_normal;
	};

	////////////////////////////////////////////////////////////////////////////////        
	void RemapData( const std::vector<int>& faceOrder );
	void ReindexTriangles(int lruSize, std::vector<int>& outIndexBuffer);
	
	////////////////////////////////////////////////////////////////////////////////        
	std::vector<Face> m_faces;
	std::vector<Vertex> m_vertices;
};

class PointGrid
{
public:
	PointGrid(const AABB &bounds, float bucketDim);

	int Find(const vec3& pt) const;
	void Add(int index, const vec3& pt);
private:
	struct VecPair {
		VecPair(const vec3& pt, int i) : vec(pt), index(i) {}
		vec3 vec;
		int index;
	};
	typedef std::vector<VecPair> VecListType;
	AABB m_bounds;
	int m_cellsx;
	int m_cellsy;
	int m_cellsz;
	std::vector<std::shared_ptr<VecListType>> m_grid;
	float m_dim;

	constexpr static float kEpsilon = 1e-4f;
	constexpr static float kEpsilonSq = kEpsilon * kEpsilon;
};

int UniqueAddVertex(TriSoup* mesh, PointGrid& grid, const vec3& pt);
