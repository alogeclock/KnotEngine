#pragma once

#include "EngineAPI.h"

#include "Components/PrimitiveComponent.h"

UCLASS()
class ENGINE_API UCubeComponent : public UPrimitiveComponent
{
	GENERATED_CLASS(UCubeComponent, UPrimitiveComponent)

public:
	UCubeComponent(URenderer& Renderer);
};
