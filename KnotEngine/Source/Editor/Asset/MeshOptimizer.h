#pragma once

#include "Asset/Mesh/StaticMesh.h"
#include "Core/CoreTypes.h"

#include <span>

// FIFO Cache 모델의 후처리 전후 비용이며 실제 GPU 시간과 구분한다.
struct FMeshOptimizationStatistics
{
	SIZE_T VertexCountBefore = 0;
	SIZE_T VertexCountAfter = 0;
	uint64 CacheMissesBefore = 0;
	uint64 CacheMissesAfterCache = 0;
	uint64 CacheMissesAfterOverdraw = 0;
};

// Cache → Overdraw → Fetch 순서로 Static Mesh의 GPU 접근 순서를 최적화한다.
class FMeshOptimizer final
{
public:
	static FMeshOptimizationStatistics Optimize(
		TArray<FStaticMeshVertex>& Vertices,
		TArray<uint32>& Indices,
		const TArray<FStaticMeshSection>& Sections,
		bool bReorderTriangles = true);

private:
	// Overdraw 정렬에 사용하는 연속 삼각형 클러스터와 바깥쪽 방향 점수다.
	struct FCluster
	{
		uint32 FirstIndex;
		uint32 IndexCount;
		float Score;
	};

	static void OptimizeVertexCache(std::span<uint32> Indices, SIZE_T VertexCount);
	static void OptimizeOverdraw(std::span<uint32> Indices, const TArray<FStaticMeshVertex>& Vertices);
	static uint64 CountCacheMisses(std::span<const uint32> Indices, SIZE_T VertexCount);
};
