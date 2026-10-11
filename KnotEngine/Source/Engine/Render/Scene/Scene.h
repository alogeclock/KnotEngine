#pragma once

#include "EngineAPI.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Render/Scene/SceneBVH.h"
#include <memory>

class FRenderer;

enum class EPrimitiveCommandAction : uint8
{
	Add,
	Update,
	Remove,
};

struct ENGINE_API FPrimitiveRenderCommand
{
	EPrimitiveCommandAction Action = EPrimitiveCommandAction::Update;
	ERenderCommandType Type = ERenderCommandType::None;
	uint64 PrimitiveId = 0;
	FPrimitiveRenderData RenderData;
	std::unique_ptr<FPrimitiveSceneProxy> Proxy;
};

// Proxy의 소유와 등록 수명을 관리한다. 원본 Component의 타입이나 데이터는 조회하지 않는다.
class ENGINE_API FScene
{
public:
	FScene() = default;
	~FScene();
	FScene(const FScene&) = delete;
	FScene& operator=(const FScene&) = delete;

	uint64 AddPrimitive(std::unique_ptr<FPrimitiveSceneProxy> Proxy, FPrimitiveRenderData&& RenderData);
	void UpdatePrimitive(uint64 PrimitiveId, ERenderCommandType Type, FPrimitiveRenderData&& RenderData);
	void RemovePrimitive(uint64 PrimitiveId);

	TArray<FPrimitiveRenderCommand> DrainRenderCommands();
	void ApplyRenderCommands(FRenderer& Renderer, TArray<FPrimitiveRenderCommand>&& Commands);

	std::shared_ptr<FSceneRayQuery> EnqueueRayQuery(const FRay& Ray, const FSceneView& View);
	TArray<std::shared_ptr<FSceneRayQuery>> DrainRayQueries();
	void ExecuteRayQueries(const TArray<std::shared_ptr<FSceneRayQuery>>& Requests);

	FSceneBVHQueryStatistics QueryFrustum(const FFrustum& Frustum, TArray<const FPrimitiveSceneProxy*>& OutPrimitives, bool bInsideOnly = false);
	FSceneBVHQueryStatistics QuerySphere(const FVector& Center, float Radius, TArray<const FPrimitiveSceneProxy*>& OutPrimitives);
	FSceneBVHQueryStatistics QueryOBB(const FOBB& Box, TArray<const FPrimitiveSceneProxy*>& OutPrimitives);
	FSceneBVHQueryStatistics QuerySegment(const FVector& Start, const FVector& End, const FVector& Extent, TArray<const FPrimitiveSceneProxy*>& OutPrimitives);

	FSceneQueryStatistics DrainQueryStatistics() { return std::exchange(QueryStatistics, {}); }
	FSceneBVH& GetBVH() { return BVH; }

private:
	static void Merge(FPrimitiveRenderData& Destination, FPrimitiveRenderData&& Source, ERenderCommandType Type);

	TArray<std::unique_ptr<FPrimitiveSceneProxy>> Proxies;
	TMap<uint64, SIZE_T> ProxyIndices;

	FSceneBVH BVH;
	FSceneQueryStatistics QueryStatistics;
	TArray<std::shared_ptr<FSceneRayQuery>> PendingRayQueries; // GT가 요청을 추가하며, 제출할 때 배열을 꺼내 RT로 넘긴다.

	TArray<FPrimitiveRenderCommand> PendingRenderCommands;
	TMap<uint64, SIZE_T> PendingCommandIndices;
	
	uint64 NextPrimitiveId = 1;
};
