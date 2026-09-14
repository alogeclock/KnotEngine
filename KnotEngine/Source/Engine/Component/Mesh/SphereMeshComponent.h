#pragma once

#include "Component/Mesh/GeometryMeshComponent.h"

UCLASS(EditorSpawnable, Category = "Geometry", DisplayName = "Sphere")
class ENGINE_API USphereMeshComponent final : public UGeometryMeshComponent
{
	GENERATED_CLASS(USphereMeshComponent, UGeometryMeshComponent)

public:
	USphereMeshComponent();
};
