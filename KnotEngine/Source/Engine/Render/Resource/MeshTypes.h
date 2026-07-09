#pragma once

#include "Render/Resource/VertexTypes.h"

struct FGeometryMesh
{
	TArray<FGeometryVertex> Vertices;
	TArray<uint32> Indices;
	FVertexLayout Layout;
};