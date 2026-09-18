#pragma once

#include "Asset/Asset/EngineAssetIds.h"
#include "Component/Mesh/StaticMeshComponent.h"

UCLASS(EditorSpawnable, Category = "Geometry", DisplayName = "Capsule")
class ENGINE_API UCapsuleComponent final : public UStaticMeshComponent
{
	GENERATED_CLASS(UCapsuleComponent, UStaticMeshComponent)

public:
	UCapsuleComponent();
};
