#include "Render/Proxy/PrimitiveSceneProxy.h"

#include "Component/Mesh/MeshComponent.h"
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
	if (!Mesh || !Mesh->GetLocalBounds().IsValid())
	{
		Mesh.reset();
	}

	// CPU Mesh가 없으면 이전 Bounds도 함께 비운다. GPU 업로드는 Renderer가 사용 직전에 수행한다.
	LocalBounds = Mesh ? Mesh->GetLocalBounds() : FAABB();
	WorldBounds = LocalBounds.IsValid() ? LocalBounds.Transform(WorldMatrix) : FAABB();
	bDirty = false;
}
