#include "Render/Resource/VertexTypes.h"

#include <cstddef>

const FVertexLayout& FGeometryVertex::GetVertexLayout()
{
	static const FVertexLayout Layout = {
		{
		    { EVertexSemantic::Position, EVertexFormat::Float3, 0, static_cast<uint16>(offsetof(FGeometryVertex, Position)) },
		    { EVertexSemantic::Color, EVertexFormat::UNorm8x4, 0, static_cast<uint16>(offsetof(FGeometryVertex, Color)) },
		},
		static_cast<uint16>(sizeof(FGeometryVertex))
	};

	return Layout;
}

const FVertexLayout& FStaticMeshVertex::GetVertexLayout()
{
	static const FVertexLayout Layout = {
		{
			{ EVertexSemantic::Position, EVertexFormat::Float3, 0, static_cast<uint16>(offsetof(FStaticMeshVertex, Position)) },
			{ EVertexSemantic::Normal, EVertexFormat::Float3, 0, static_cast<uint16>(offsetof(FStaticMeshVertex, Normal)) },
			{ EVertexSemantic::Tangent, EVertexFormat::Float4, 0, static_cast<uint16>(offsetof(FStaticMeshVertex, Tangent)) },
			{ EVertexSemantic::TexCoord0, EVertexFormat::Float2, 0, static_cast<uint16>(offsetof(FStaticMeshVertex, TexCoord)) },
		},
		static_cast<uint16>(sizeof(FStaticMeshVertex))
	};

	return Layout;
}
