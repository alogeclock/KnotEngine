#pragma once

#include "Asset/Mesh/StaticMesh.h"
#include "Component/PrimitiveComponent.h"

#include <memory>

// 경로로 로드된 Static Mesh Asset을 Scene에 제출하는 Primitive Component다.
UCLASS(EditorSpawnable, Category = "Mesh", DisplayName = "Static Mesh")
class ENGINE_API UStaticMeshComponent : public UPrimitiveComponent
{
	GENERATED_CLASS(UStaticMeshComponent, UPrimitiveComponent)

public:
	UStaticMeshComponent() = default;

	void SetStaticMesh(UStaticMesh* InStaticMesh);
	UStaticMesh* GetStaticMesh() const { return StaticMesh.Get(); }

protected:
	explicit UStaticMeshComponent(const char* DefaultAssetPath);
	std::unique_ptr<FPrimitiveSceneProxy> CreatePrimitiveSceneProxy() const override;

private:
	UPROPERTY(Category = "Static Mesh")
	TObjectPtr<UStaticMesh> StaticMesh;
};
