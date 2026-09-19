#include "Component/PrimitiveComponent.h"
#include "Render/Scene/Scene.h"
#include "World/World.h"

void UPrimitiveComponent::OnRegister()
{
	check(!SceneProxy);
	SceneProxy = &GetWorld().GetScene().AddPrimitive(CreatePrimitiveSceneProxy());
}

void UPrimitiveComponent::OnUnregister()
{
	check(SceneProxy);
	GetWorld().GetScene().RemovePrimitive(*SceneProxy);
	SceneProxy = nullptr;
}

void UPrimitiveComponent::MarkPrimitiveSceneProxy()
{
	if (SceneProxy)
	{
		SceneProxy->bDirty = true;
	}
}

// Node 선택 변경 시 등록된 Scene Proxy의 렌더 전용 선택 Cache를 갱신한다.
void UPrimitiveComponent::PushSelection(bool bSelected)
{
	if (SceneProxy)
	{
		SceneProxy->bSelected = bSelected;
	}
}

void UPrimitiveComponent::SetVisible(bool bInVisible)
{
	if (bVisible != bInVisible)
	{
		bVisible = bInVisible;
		MarkPrimitiveSceneProxy();
	}
}

void UPrimitiveComponent::PostEditProperty(const FProperty& Property)
{
	Super::PostEditProperty(Property);
	MarkPrimitiveSceneProxy();
}
