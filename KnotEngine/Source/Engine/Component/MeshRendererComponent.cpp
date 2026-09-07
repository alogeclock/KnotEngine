#include "Component/MeshRendererComponent.h"

#include "Component/TransformComponent.h"
#include "Render/Renderer.h"
#include "Render/Resource/MeshTypes.h"

#include <utility>

void UMeshRendererComponent::SetMesh(std::shared_ptr<FGeometryMesh> InMesh)
{
	Mesh = std::move(InMesh);
}

void UMeshRendererComponent::Render(URenderer& Renderer, const FMatrix& ViewProjection) const
{
	if (!IsVisible() || !Mesh || !Mesh->GetMeshBuffer())
	{
		return;
	}
	Renderer.UpdateConstant(GetTransform().GetWorldMatrix() * ViewProjection);
	Renderer.DrawMeshBuffer(*Mesh->GetMeshBuffer());
}
