#pragma once

#include "Asset/Asset/EngineAssetIds.h"
#include "Component/Mesh/StaticMeshComponent.h"

UCLASS(EditorSpawnable, Category = "Geometry", DisplayName = "Sphere")
class ENGINE_API USphereComponent final : public UStaticMeshComponent
{
	GENERATED_CLASS(USphereComponent, UStaticMeshComponent)

public:
	USphereComponent();
};
