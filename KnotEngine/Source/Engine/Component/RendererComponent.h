#pragma once

#include "Component/Component.h"

class URenderer;
struct FMatrix;

UCLASS()
class ENGINE_API URendererComponent : public UComponent
{
	GENERATED_CLASS(URendererComponent, UComponent)

public:
	bool IsVisible() const { return bVisible; }
	void SetVisible(bool bInVisible) { bVisible = bInVisible; }

	virtual void Render(URenderer& Renderer, const FMatrix& ViewProjection) const = 0;

private:
	UPROPERTY(Category = "Rendering") bool bVisible = true;
};
