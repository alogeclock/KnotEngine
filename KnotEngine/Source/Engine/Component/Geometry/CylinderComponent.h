#pragma once

#include "Component/Mesh/StaticMeshComponent.h"

UCLASS(EditorSpawnable, Category = "Geometry", DisplayName = "Cylinder")
class ENGINE_API UCylinderComponent final : public UStaticMeshComponent
{
	GENERATED_CLASS(UCylinderComponent, UStaticMeshComponent)

public:
	UCylinderComponent();

private:
	static constexpr const char* StaticMeshPath = "/Engine/Geometry/Cylinder";
};
