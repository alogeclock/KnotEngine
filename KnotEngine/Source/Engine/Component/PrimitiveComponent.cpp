#include "Component/PrimitiveComponent.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Render/Scene/Scene.h"
#include "World/World.h"

void UPrimitiveComponent::OnRegister()
{
	check(PrimitiveId == InvalidPrimitiveId);
	FPrimitiveRenderData RenderData = BuildPrimitiveRenderData(ERenderCommandType::All);
	PrimitiveId = GetWorld().GetScene().AddPrimitive(CreatePrimitiveSceneProxy(RenderData), std::move(RenderData));
}

void UPrimitiveComponent::OnUnregister()
{
	check(PrimitiveId != InvalidPrimitiveId);
	GetWorld().GetScene().RemovePrimitive(PrimitiveId);
	PrimitiveId = InvalidPrimitiveId;
}

void UPrimitiveComponent::EnqueueRenderCommand(ERenderCommandType Type)
{
	if (PrimitiveId != InvalidPrimitiveId)
	{
		GetWorld().GetScene().UpdatePrimitive(PrimitiveId, Type, BuildPrimitiveRenderData(Type));
	}
}

// Node 선택 변경 시 등록된 Scene Proxy의 렌더 전용 선택 Cache를 갱신한다.
void UPrimitiveComponent::PushSelection(bool bSelected)
{
	EnqueueRenderCommand(ERenderCommandType::Visibility);
}

void UPrimitiveComponent::SetVisible(bool bInVisible)
{
	if (bVisible != bInVisible)
	{
		bVisible = bInVisible;
		EnqueueRenderCommand(ERenderCommandType::Visibility);
	}
}

void UPrimitiveComponent::PostEditProperty(const FProperty& Property)
{
	Super::PostEditProperty(Property);
	EnqueueRenderCommand();
}
