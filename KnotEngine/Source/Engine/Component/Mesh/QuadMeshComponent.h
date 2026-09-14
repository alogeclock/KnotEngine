#pragma once

#include "Component/Mesh/GeometryMeshComponent.h"

UCLASS(EditorSpawnable, Category = "Geometry", DisplayName = "Quad")
class ENGINE_API UQuadMeshComponent final : public UGeometryMeshComponent
{
	GENERATED_CLASS(UQuadMeshComponent, UGeometryMeshComponent)

public:
	UQuadMeshComponent();
};
