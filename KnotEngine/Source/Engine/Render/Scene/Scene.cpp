#include "Render/Scene/Scene.h"
#include "Core/Assert.h"
#include <algorithm>

FScene::~FScene()
{
	// World는 Scene보다 먼저 Component 등록을 해제하여 Proxy를 제거한다.
	check(Proxies.empty());
}

FPrimitiveSceneProxy& FScene::AddPrimitive(std::unique_ptr<FPrimitiveSceneProxy> Proxy)
{
	check(Proxy);
	Proxies.push_back(std::move(Proxy));
	return *Proxies.back();
}

void FScene::RemovePrimitive(FPrimitiveSceneProxy& Proxy)
{
	const auto Iterator = std::find_if(Proxies.begin(), Proxies.end(), [&Proxy](const auto& Entry) { return Entry.get() == &Proxy; });
	check(Iterator != Proxies.end());
	Proxies.erase(Iterator);
}

// World 갱신이 끝난 뒤 각 Proxy가 자신의 Dirty 상태를 검사하고 필요한 데이터만 갱신한다.
void FScene::Update()
{
	for (const auto& Proxy : Proxies)
	{
		Proxy->Update();
	}
}
