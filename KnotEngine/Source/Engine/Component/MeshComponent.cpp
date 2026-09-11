#include "Component/MeshComponent.h"

#include "Render/Proxy/PrimitiveSceneProxy.h"

#include <utility>

void UMeshComponent::SetMesh(std::shared_ptr<FGeometryMesh> InMesh)
{
	Mesh = std::move(InMesh);
	MarkPrimitiveSceneProxy();
}

std::unique_ptr<FPrimitiveSceneProxy> UMeshComponent::CreatePrimitiveSceneProxy() const
{
	return std::make_unique<FPrimitiveSceneProxy>(*this);
}
