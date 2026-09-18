#pragma once

#include "Asset/Material/MaterialInterface.h"
#include "Asset/Mesh/StaticMesh.h"
#include "Component/PrimitiveComponent.h"

#include <memory>

// 영속 ID로 해결된 Static Mesh Asset을 Scene에 제출하는 Primitive Component다.
UCLASS(EditorSpawnable, Category = "Mesh", DisplayName = "Static Mesh")
class ENGINE_API UStaticMeshComponent : public UPrimitiveComponent
{
	GENERATED_CLASS(UStaticMeshComponent, UPrimitiveComponent)

public:
	UStaticMeshComponent() = default;

	void SetStaticMesh(UStaticMesh* InStaticMesh);
	UStaticMesh* GetStaticMesh() const { return StaticMesh.Get(); }
	void PostEditProperty(const FProperty& Property) override;

	void SetMaterial(SIZE_T MaterialIndex, UMaterialInterface* InMaterial);

	UMaterialInterface* GetMaterial(SIZE_T MaterialIndex) const;
	SIZE_T GetMaterialCount() const;
	const TArray<TObjectPtr<UMaterialInterface>>& GetOverrideMaterials() const { return OverrideMaterials; }

protected:
	explicit UStaticMeshComponent(const FAssetId& DefaultAssetId);
	std::unique_ptr<FPrimitiveSceneProxy> CreatePrimitiveSceneProxy() const override;

private:
	UPROPERTY(Category = "Static Mesh")
	TObjectPtr<UStaticMesh> StaticMesh;

	UPROPERTY(Category = "Material")
	TArray<TObjectPtr<UMaterialInterface>> OverrideMaterials;
};
