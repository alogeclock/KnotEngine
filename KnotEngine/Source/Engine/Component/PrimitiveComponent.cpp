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
