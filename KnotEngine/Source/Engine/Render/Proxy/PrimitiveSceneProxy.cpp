#include "Render/Proxy/PrimitiveSceneProxy.h"

#include "Component/Mesh/StaticMeshComponent.h"
#include "Component/TransformComponent.h"
#include "Render/Resource/MeshTypes.h"

FPrimitiveSceneProxy::FPrimitiveSceneProxy(const UPrimitiveComponent& InComponent)
	: Component(InComponent)
{
}

void FPrimitiveSceneProxy::UpdateBounds(const FAABB& InLocalBounds)
{
	// Component와의 friend 관계로 현재 상태를 읽는다. 갱신 중 임시 Proxy를 생성하지 않는다.
	WorldMatrix = Component.GetTransform().GetWorldMatrix();
	bVisible = Component.bVisible;
	LocalBounds = InLocalBounds;
	WorldBounds = LocalBounds.IsValid() ? LocalBounds.Transform(WorldMatrix) : FAABB();
	bDirty = false;
}

FStaticMeshSceneProxy::FStaticMeshSceneProxy(const UStaticMeshComponent& InComponent)
	: FPrimitiveSceneProxy(InComponent), MeshComponent(InComponent)
{
	Update();
}

void FStaticMeshSceneProxy::Update()
{
	if (!bDirty)
	{
		return;
	}

	Mesh = MeshComponent.GetStaticMesh() ? &MeshComponent.GetStaticMesh()->GetRenderData() : nullptr;
	if (Mesh && !Mesh->IsValid())
	{
		Mesh = nullptr;
	}
	UpdateBounds(Mesh ? Mesh->GetLocalBounds() : FAABB());
}
