#include "Render/Scene/Scene.h"
#include "Core/Assert.h"
FScene::~FScene()
{
	// World는 Scene보다 먼저 Component 등록을 해제하여 Proxy를 제거한다.
	check(Proxies.empty());
}

FPrimitiveSceneProxy& FScene::AddPrimitive(std::unique_ptr<FPrimitiveSceneProxy> Proxy)
{
	check(Proxy);
	check(Proxy->SceneIndex == FPrimitiveSceneProxy::InvalidSceneIndex);
	Proxy->SceneIndex = Proxies.size();
	Proxies.push_back(std::move(Proxy));
	return *Proxies.back();
}

void FScene::RemovePrimitive(FPrimitiveSceneProxy& Proxy)
{
	check(Proxy.SceneIndex < Proxies.size());
	check(Proxies[Proxy.SceneIndex].get() == &Proxy);

	const SIZE_T RemoveIndex = Proxy.SceneIndex;
	const SIZE_T LastIndex = Proxies.size() - 1;
	if (RemoveIndex != LastIndex)
	{
		std::swap(Proxies[RemoveIndex], Proxies[LastIndex]);
		Proxies[RemoveIndex]->SceneIndex = RemoveIndex;
	}
	Proxy.SceneIndex = FPrimitiveSceneProxy::InvalidSceneIndex;
	Proxies.pop_back();
}

// World 갱신이 끝난 뒤 각 Proxy가 자신의 Dirty 상태를 검사하고 필요한 데이터만 갱신한다.
void FScene::Update()
{
	for (const auto& Proxy : Proxies)
	{
		Proxy->Update();
	}
}
