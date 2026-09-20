#pragma once

#include "Core/CoreTypes.h"
#include "Core/Geometry/Edge.h"
#include "Asset/Mesh/StaticMesh.h"

#include <limits>

// Import 과정에서 생성한 단일 Static Mesh LOD의 직렬화 전 CPU 데이터다.
struct FStaticMeshLODBuildData
{
	TArray<FStaticMeshVertex> Vertices;
	TArray<uint32> Indices;
	TArray<FStaticMeshSection> Sections;
};

// 원본 LOD와 생성할 하위 LOD의 삼각형 비율을 지정한다.
struct FMeshSimplificationDesc
{
	const TArray<FStaticMeshVertex>& Vertices;
	const TArray<uint32>& Indices;
	const TArray<FStaticMeshSection>& Sections;
	const TArray<float>& TriangleRatios;
};

// Quadric Error Metric 기반 Edge Collapse로 Static Mesh의 하위 LOD를 생성한다.
class FMeshSimplifier final
{
public:
	static TArray<FStaticMeshLODBuildData> GenerateLODs(const FMeshSimplificationDesc& Desc);

private:
	struct FQuadric
	{
		float Values[10] = {};

		FQuadric& operator+=(const FQuadric& Other);
		float Evaluate(const FVector& Position) const;
		void AddPlane(const FVector& Normal, float Distance, float Weight);
	};

	struct FTopologicalVertex
	{
		FVector Position;
		TArray<uint32> RenderVertices;
		TArray<uint32> Triangles;
		FQuadric Quadric;
		uint32 Version = 0;
		bool bBoundary = false;
		bool bDeleted = false;
	};

	struct FCollapseCandidate
	{
		FIndexEdge Edge;
		FVector Position;
		float Error = (std::numeric_limits<float>::max)();
		uint32 VersionA = 0;
		uint32 VersionB = 0;

		bool operator<(const FCollapseCandidate& Other) const { return Error > Other.Error; }
	};

	struct FIndexEdgeHasher
	{
		SIZE_T operator()(const FIndexEdge& Edge) const noexcept
		{
			const FIndexEdge CanonicalEdge = Edge.Canonical();
			return static_cast<SIZE_T>((static_cast<uint64>(CanonicalEdge.A) << 32) | CanonicalEdge.B);
		}
	};

	explicit FMeshSimplifier(const FMeshSimplificationDesc& Desc);

	void BuildTopology();
	void BuildQuadricsAndEdges();

	FCollapseCandidate BuildCollapseCandidate(uint32 VertexA, uint32 VertexB) const;
	bool WouldInvertTriangle(uint32 VertexA, uint32 VertexB, const FVector& Position) const;
	bool Collapse(const FCollapseCandidate& Candidate, TArray<uint32>& Neighbors);

	FStaticMeshLODBuildData BuildCurrentLOD() const;

	TArray<FStaticMeshVertex> Vertices;

	const TArray<uint32>& SourceIndices;
	const TArray<FStaticMeshSection>& SourceSections;

	TArray<uint32> TopologicalIndices;
	TArray<FTopologicalVertex> TopologicalVertices;
	TSet<FIndexEdge, FIndexEdgeHasher> Edges;

	TArray<bool> AliveTriangles;

	int32 TriangleCount = 0;
};
