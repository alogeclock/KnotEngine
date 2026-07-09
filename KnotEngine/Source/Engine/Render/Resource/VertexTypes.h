#pragma once

#include "Render/Resource/VertexLayouts.h"

constexpr uint32 PackColor(uint8 R, uint8 G, uint8 B, uint8 A = 255)
{
    return uint32(R) | (uint32(G) << 8) | (uint32(B) << 16) | (uint32(A) << 24);
}

// 선, 큐브, 삼각형 등 단순 디버그용 Geometry를 그리기 위한 Vertex 구조체
struct FGeometryVertex
{
    FVector Position;
    uint32 Color;

    static constexpr uint32 Stride = 16;
    static constexpr FVertexElement Elements[] = {
        { FVertexSemantic::Position, FVertexFormat::Float3, 0, 0, 0 },
        { FVertexSemantic::Color, FVertexFormat::UNorm8x4, 0, 0, 12 },
    };
};
static_assert(sizeof(FGeometryVertex) == 16, "FGeometryVertex size must be 16 bytes.");
