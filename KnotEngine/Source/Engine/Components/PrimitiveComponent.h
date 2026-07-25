#pragma once

#include "Object/Object.h"
#include "Render/Resource/MeshTypes.h"

class URenderer;

class UPrimitiveComponent : public UObject
{
public:
    ~UPrimitiveComponent() override = default;

    void Render(float DeltaTime, URenderer& Renderer);

protected:
    std::shared_ptr<FGeometryMesh> Mesh;
    float Rotation = 0.0f;
};
