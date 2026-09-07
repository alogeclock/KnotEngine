#pragma once

#include "Component/RendererComponent.h"

#include <memory>

class FGeometryMesh;

UCLASS()
class ENGINE_API UMeshRendererComponent : public URendererComponent
{
	GENERATED_CLASS(UMeshRendererComponent, URendererComponent)

public:
	void SetMesh(std::shared_ptr<FGeometryMesh> InMesh);
	const std::shared_ptr<FGeometryMesh>& GetMesh() const { return Mesh; }
	void Render(URenderer& Renderer, const FMatrix& ViewProjection) const override;

protected:
	std::shared_ptr<FGeometryMesh> Mesh;
};
