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
	UStaticMeshComponent();

	void SetStaticMesh(UStaticMesh* InStaticMesh);
	UStaticMesh* GetStaticMesh() const { return StaticMesh.Get(); }
	void PostEditProperty(const FProperty& Property) override;

	void SetMaterial(SIZE_T MaterialIndex, UMaterialInterface* InMaterial);

	UMaterialInterface* GetMaterial(SIZE_T MaterialIndex) const;
	SIZE_T GetMaterialCount() const;
	const TArray<TObjectPtr<UMaterialInterface>>& GetOverrideMaterials() const { return OverrideMaterials; }
	bool IsLODEnable() const { return bLODEnable; }
	FPrimitiveRenderData BuildPrimitiveRenderData(ERenderCommandType Type) const override;

protected:
	explicit UStaticMeshComponent(const FAssetId& DefaultAssetId);
	std::unique_ptr<FPrimitiveSceneProxy> CreatePrimitiveSceneProxy(const FPrimitiveRenderData& RenderData) const override;

private:
	/// 이 컴포넌트가 렌더링할 Static Mesh Asset이다.
	UPROPERTY(Category = "Static Mesh") TObjectPtr<UStaticMesh> StaticMesh;

	/// 화면 점유율에 따른 LOD 선택을 사용할지 결정한다.
	UPROPERTY(Category = "LOD") bool bLODEnable = true;

	/// Static Mesh의 Material Slot별 재정의 Material 목록이다.
	UPROPERTY(Category = "Material") TArray<TObjectPtr<UMaterialInterface>> OverrideMaterials;
};
