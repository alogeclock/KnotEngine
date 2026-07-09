#pragma once

#include "Render/Resource/VertexLayouts.h"

// GPU 메모리에 올라가는 Mesh Resource 구조체.
struct FGeometryMeshResource
{
    ID3D11Buffer* VertexBuffer = nullptr;
    ID3D11Buffer* IndexBuffer = nullptr;

    uint32 VertexCount = 0;
    uint32 IndexCount = 0;
    uint32 Stride = 0;

    FVertexLayout Layout;
};

struct FStaticMeshResource
{
};

struct FSkeletalMeshResource
{
};