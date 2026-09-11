#pragma once

#include "EngineAPI.h"

#include "Component/MeshComponent.h"

class URenderer;

// TO-DO: 추후 삭제할 컴포넌트
UCLASS()
class ENGINE_API UCubeComponent : public UMeshComponent
{
	GENERATED_CLASS(UCubeComponent, UMeshComponent)

public:
	UCubeComponent(URenderer& Renderer);
};
