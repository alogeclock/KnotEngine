#pragma once

#include "Component/Mesh/StaticMeshComponent.h"

UCLASS(EditorSpawnable, Category = "Geometry", DisplayName = "Capsule")
class ENGINE_API UCapsuleComponent final : public UStaticMeshComponent
{
	GENERATED_CLASS(UCapsuleComponent, UStaticMeshComponent)

public:
	UCapsuleComponent();

private:
	static constexpr const char* StaticMeshPath = "/Engine/Model/Capsule/Capsule";
};
