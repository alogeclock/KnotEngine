#pragma once

#include "Component/Mesh/GeometryMeshComponent.h"

UCLASS(EditorSpawnable, Category = "Geometry", DisplayName = "Cube")
class ENGINE_API UCubeMeshComponent final : public UGeometryMeshComponent
{
	GENERATED_CLASS(UCubeMeshComponent, UGeometryMeshComponent)

public:
	UCubeMeshComponent();
};
