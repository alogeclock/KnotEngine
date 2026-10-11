#pragma once

#include "EngineAPI.h"
#include "Core/Geometry/AABB.h"
#include "Core/Geometry/Frustum.h"
#include "Core/Geometry/OBB.h"
#include "Core/Geometry/Ray.h"
#include "Render/Scene/SceneView.h"
#include <atomic>
#include <memory>

// GT가 입력 시점의 Ray와 View를 제출하고 RT가 결과 작성 후 bReady를 설정하면, GT가 완료 여부를 확인한 뒤 결과를 읽는다.
struct FSceneRayQuery
{
	FRay Ray;
	FSceneView View;

	std::atomic<bool> bReady = false;
	uint64 PrimitiveId = 0;
	FVector HitPosition = FVector::ZeroVector;
};

struct FSceneBVHQueryStatistics
{
	uint64 QueryCount = 0;
	uint64 BoundsTests = 0;
	uint64 Candidates = 0;
	double TimeMs = 0.0;
};

// RT가 렌더 프레임 안의 Query 통계를 종류별로 합산하며, GT에는 완료된 프레임의 복사본만 전달한다.
struct FSceneQueryStatistics
{
	FSceneBVHQueryStatistics Frustum;
	FSceneBVHQueryStatistics Sphere;
	FSceneBVHQueryStatistics OBB;
	FSceneBVHQueryStatistics Segment;
};

struct FSceneBVHRayHit
{
	uint64 PrimitiveId = 0;
	float Distance = 0.0f; // Ray가 Bounds에 처음 들어가는 t 값이며, Origin + Direction * t가 교차 위치다. 메시 표면 교차는 별도로 검사한다.
};

// RT Debug Draw에 전달할 노드 Bounds와 Root부터의 깊이이며 같은 깊이는 같은 색으로 표시한다.
struct FSceneBVHDebugBounds
{
	FAABB Bounds;
	uint32 Depth = 0;
};

// RT에서 Primitive ID와 World AABB를 저장하고, Ray·Frustum·Sphere 등과 겹치는 Bounds를 찾는다. GT는 직접 접근하지 않는다.
class ENGINE_API FSceneBVH
{
public:
	void Update(uint64 PrimitiveId, const FAABB& Bounds);
	void Remove(uint64 PrimitiveId);
	void Prepare();

	void CollectDebugBounds(TArray<FSceneBVHDebugBounds>& OutBounds);

	void RayQuery(const FRay& Ray, TArray<FSceneBVHRayHit>& OutHits, FSceneBVHQueryStatistics& Statistics);
	void FrustumQuery(const FFrustum& Frustum, TArray<uint64>& OutIds, FSceneBVHQueryStatistics& Statistics, bool bInsideOnly = false);
	void SphereQuery(const FVector& Center, float Radius, TArray<uint64>& OutIds, FSceneBVHQueryStatistics& Statistics);
	void OBBQuery(const FOBB& Box, TArray<uint64>& OutIds, FSceneBVHQueryStatistics& Statistics);
	void SegmentQuery(const FVector& Start, const FVector& End, const FVector& Extent, TArray<uint64>& OutIds, FSceneBVHQueryStatistics& Statistics);

private:
	struct FEntry
	{
		uint64 PrimitiveId;
		FAABB Bounds;
		int32 Leaf = -1;
	};

	struct FNode
	{
		FAABB Bounds;
		int32 Parent = -1;
		int32 Left = -1;
		int32 Right = -1;
		int32 Entry = -1;
	};

	int32 Build(TArray<int32>& Order, SIZE_T Begin, SIZE_T End, int32 Parent);
	void Refit(int32 NodeIndex);

	TArray<FEntry> Entries;
	TMap<uint64, SIZE_T> EntryIndices;
	TArray<FNode> Nodes;
	TArray<int32> TraversalStack;

	int32 Root = -1;
	bool bTopologyDirty = false;
};
