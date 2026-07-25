#include "Render/Resource/VertexTypes.h"

#include <cstddef>

const FVertexLayout& FGeometryVertex::GetVertexLayout()
{
    static const FVertexLayout Layout = {
        {
            { FVertexSemantic::Position, FVertexFormat::Float3, 0, static_cast<uint16>(offsetof(FGeometryVertex, Position)) },
            { FVertexSemantic::Color, FVertexFormat::UNorm8x4, 0, static_cast<uint16>(offsetof(FGeometryVertex, Color)) },
        },
        static_cast<uint16>(sizeof(FGeometryVertex))
    };

    return Layout;
}
