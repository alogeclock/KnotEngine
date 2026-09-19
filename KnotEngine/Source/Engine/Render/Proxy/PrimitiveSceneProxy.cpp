#include "Render/Proxy/PrimitiveSceneProxy.h"

#include "Component/Mesh/StaticMeshComponent.h"
#include "Component/TransformComponent.h"
#include "Render/Resource/Mesh/Mesh.h"
#include "World/Node.h"

FPrimitiveSceneProxy::FPrimitiveSceneProxy(const UPrimitiveComponent& InComponent)
	: Component(InComponent), bSelected(InComponent.GetOwner().IsSelected())
{
}

UNode& FPrimitiveSceneProxy::GetOwner() const
{
	return Component.GetOwner();
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
	Materials.clear();
	if (Mesh && !Mesh->IsValid())
	{
		Mesh = nullptr;
	}
	if (Mesh)
	{
		Materials.resize(MeshComponent.GetMaterialCount());
		for (SIZE_T MaterialIndex = 0; MaterialIndex < Materials.size(); ++MaterialIndex)
		{
			Materials[MaterialIndex] = MeshComponent.GetMaterial(MaterialIndex);
		}
	}
	UpdateBounds(Mesh ? Mesh->GetLocalBounds() : FAABB());
}
