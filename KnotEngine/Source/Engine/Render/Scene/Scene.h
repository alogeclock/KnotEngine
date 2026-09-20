#pragma once

#include "EngineAPI.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include <memory>

class UPrimitiveComponent;
class URenderer;
class FSceneRenderer;

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
	void ApplyRenderCommands(URenderer& Renderer, TArray<FPrimitiveRenderCommand>&& Commands);

private:
	friend class FSceneRenderer;
	const TArray<std::unique_ptr<FPrimitiveSceneProxy>>& GetProxies() const { return Proxies; }
	static void Merge(FPrimitiveRenderData& Destination, FPrimitiveRenderData&& Source, ERenderCommandType Type);

	TArray<std::unique_ptr<FPrimitiveSceneProxy>> Proxies;
	TMap<uint64, SIZE_T> ProxyIndices;
	TArray<FPrimitiveRenderCommand> PendingRenderCommands;
	uint64 NextPrimitiveId = 1;
};
