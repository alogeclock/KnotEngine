#pragma once

#include "Component/PrimitiveComponent.h"

#include <memory>

class FGeometryMesh;

UCLASS()
class ENGINE_API UMeshComponent : public UPrimitiveComponent
{
	GENERATED_CLASS(UMeshComponent, UPrimitiveComponent)

public:
	void SetMesh(std::shared_ptr<FGeometryMesh> InMesh);
	const std::shared_ptr<FGeometryMesh>& GetMesh() const { return Mesh; }

protected:
	friend struct FPrimitiveSceneProxy;
	std::unique_ptr<FPrimitiveSceneProxy> CreatePrimitiveSceneProxy() const override;
	std::shared_ptr<FGeometryMesh> Mesh;
};
