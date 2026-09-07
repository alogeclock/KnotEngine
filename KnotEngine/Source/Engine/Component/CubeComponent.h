#pragma once

#include "EngineAPI.h"

#include "Component/MeshRendererComponent.h"

UCLASS()
class ENGINE_API UCubeComponent : public UMeshRendererComponent
{
	GENERATED_CLASS(UCubeComponent, UMeshRendererComponent)

public:
	UCubeComponent(URenderer& Renderer);
};
