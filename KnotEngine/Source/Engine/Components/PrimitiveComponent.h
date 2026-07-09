#pragma once

#include "Object/Object.h"
#include "Render/Renderer.h"

class UPrimitiveComponent : public UObject
{
public:
    ~UPrimitiveComponent() override;

    void Render(float DeltaTime, URenderer& Renderer);

protected:
    void ReleaseVertexBuffer();

protected:
	ID3D11Buffer* VertexBuffer = nullptr;
    UINT VertexCount = 0;
    float Rotation = 0.0f;
};
