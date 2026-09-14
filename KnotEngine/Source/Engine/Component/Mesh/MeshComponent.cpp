#include "Component/Mesh/MeshComponent.h"

#include "Render/Proxy/PrimitiveSceneProxy.h"

void UMeshComponent::SetMesh(UGeometryMesh* InMesh)
{
	Mesh = InMesh;
	MarkPrimitiveSceneProxy();
}

std::unique_ptr<FPrimitiveSceneProxy> UMeshComponent::CreatePrimitiveSceneProxy() const
{
	return std::make_unique<FPrimitiveSceneProxy>(*this);
}
