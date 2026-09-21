#include "Render/Resource/Mesh/Vertex.h"

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

const FVertexLayout& FStaticMeshInstance::GetVertexLayout()
{
	static const FVertexLayout Layout = {
		{
			{ EVertexSemantic::InstanceModel, EVertexFormat::Float4, 0, 0, 1, EVertexInputRate::PerInstance, 1 },
			{ EVertexSemantic::InstanceModel, EVertexFormat::Float4, 1, 16, 1, EVertexInputRate::PerInstance, 1 },
			{ EVertexSemantic::InstanceModel, EVertexFormat::Float4, 2, 32, 1, EVertexInputRate::PerInstance, 1 },
			{ EVertexSemantic::InstanceModel, EVertexFormat::Float4, 3, 48, 1, EVertexInputRate::PerInstance, 1 },
		},
		static_cast<uint16>(sizeof(FStaticMeshInstance))
	};

	return Layout;
}
