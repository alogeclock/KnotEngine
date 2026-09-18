#pragma once

#include "Asset/Asset/EngineAssetIds.h"
#include "Component/Mesh/StaticMeshComponent.h"

UCLASS(EditorSpawnable, Category = "Geometry", DisplayName = "Quad")
class ENGINE_API UQuadComponent final : public UStaticMeshComponent
{
	GENERATED_CLASS(UQuadComponent, UStaticMeshComponent)

public:
	UQuadComponent();
};
