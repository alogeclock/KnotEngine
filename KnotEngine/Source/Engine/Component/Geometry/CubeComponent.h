#pragma once

#include "Component/Mesh/StaticMeshComponent.h"

UCLASS(EditorSpawnable, Category = "Geometry", DisplayName = "Cube")
class ENGINE_API UCubeComponent final : public UStaticMeshComponent
{
	GENERATED_CLASS(UCubeComponent, UStaticMeshComponent)

public:
	UCubeComponent();

private:
	static constexpr const char* StaticMeshPath = "/Engine/Model/Cube/Cube";
};
