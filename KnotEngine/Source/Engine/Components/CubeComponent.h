#pragma once

#include "Components/PrimitiveComponent.h"

UCLASS()
class UCubeComponent : public UPrimitiveComponent
{
	GENERATED_CLASS(UCubeComponent, UPrimitiveComponent)

public:
	UCubeComponent(URenderer& Renderer);
};
