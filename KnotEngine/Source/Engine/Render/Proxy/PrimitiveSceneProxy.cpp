#include "Render/Proxy/PrimitiveSceneProxy.h"

#include "Component/MeshComponent.h"
#include "Component/TransformComponent.h"
#include "Render/Resource/MeshResources.h"
#include "Render/Resource/MeshTypes.h"

FPrimitiveSceneProxy::FPrimitiveSceneProxy(const UMeshComponent& InComponent)
	: Component(InComponent)
{
	Update();
}

void FPrimitiveSceneProxy::Update()
{
	if (!bDirty)
	{
		return;
	}

	// Component와의 friend 관계로 현재 상태를 읽는다. 갱신 중 임시 Proxy를 생성하지 않는다.
	WorldMatrix = Component.GetTransform().GetWorldMatrix();
	bVisible = Component.bVisible;
	Mesh = Component.Mesh;
	if (!Mesh || !Mesh->GetMeshBuffer() || !Mesh->GetMeshBuffer()->IsValid())
	{
		Mesh.reset();
	}

	// Mesh가 해제되거나 아직 업로드되지 않았으면 이전 Bounds도 함께 비운다.
	LocalBounds = Mesh ? Mesh->GetLocalBounds() : FAABB();
	WorldBounds = LocalBounds.IsValid() ? LocalBounds.Transform(WorldMatrix) : FAABB();
	bDirty = false;
}
