#pragma once

#include "EngineAPI.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include <memory>

// Proxy의 소유와 등록 수명을 관리한다. 원본 Component의 타입이나 데이터는 조회하지 않는다.
class ENGINE_API FScene
{
public:
	FScene() = default;
	~FScene();
	FScene(const FScene&) = delete;
	FScene& operator=(const FScene&) = delete;

	void Update();

	FPrimitiveSceneProxy& AddPrimitive(std::unique_ptr<FPrimitiveSceneProxy> Proxy);
	void RemovePrimitive(FPrimitiveSceneProxy& Proxy);
	const TArray<std::unique_ptr<FPrimitiveSceneProxy>>& GetProxies() const { return Proxies; }

private:
	TArray<std::unique_ptr<FPrimitiveSceneProxy>> Proxies;
};
