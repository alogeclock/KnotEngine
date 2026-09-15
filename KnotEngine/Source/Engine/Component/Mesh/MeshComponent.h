#pragma once

#include "Asset/GeometryMesh.h"
#include "Component/PrimitiveComponent.h"

#include <memory>

UCLASS()
class ENGINE_API UMeshComponent : public UPrimitiveComponent
{
	GENERATED_CLASS(UMeshComponent, UPrimitiveComponent)

public:
	void SetMesh(UGeometryMesh* InMesh);
	UGeometryMesh* GetMesh() const { return Mesh.Get(); }

protected:
	friend struct FPrimitiveSceneProxy;
	std::unique_ptr<FPrimitiveSceneProxy> CreatePrimitiveSceneProxy() const override;
	UPROPERTY(Category = "Primitive") TObjectPtr<UGeometryMesh> Mesh;
};
