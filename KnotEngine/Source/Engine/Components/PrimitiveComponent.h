#pragma once

#include "EngineAPI.h"

#include "Object/Object.h"

#include <memory>

class FGeometryMesh;
class URenderer;

UCLASS()
class ENGINE_API UPrimitiveComponent : public UObject
{
	GENERATED_CLASS(UPrimitiveComponent, UObject)

public:
	~UPrimitiveComponent() override = default;

	void Render(float DeltaTime, URenderer& Renderer);

protected:
	std::shared_ptr<FGeometryMesh> Mesh;
	float Rotation = 0.0f;
};
