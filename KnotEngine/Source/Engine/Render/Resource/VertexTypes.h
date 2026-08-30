#pragma once

#include "Core/Math/Vector.h"
#include "Render/RHI/VertexLayout.h"

#include <type_traits>

constexpr uint32 PackRGBA(uint8 R, uint8 G, uint8 B, uint8 A = 255)
{
	return uint32(R) | (uint32(G) << 8) | (uint32(B) << 16) | (uint32(A) << 24);
}

// 선, 큐브, 삼각형 등 단순 디버그용 Geometry를 그리기 위한 Vertex 구조체
struct FGeometryVertex
{
	FVector Position;
	uint32 Color;

	static const FVertexLayout& GetVertexLayout();
};
static_assert(std::is_standard_layout_v<FGeometryVertex>, "FGeometryVertex must have a standard layout.");
static_assert(std::is_trivially_copyable_v<FGeometryVertex>, "FGeometryVertex must be trivially copyable.");
static_assert(sizeof(FGeometryVertex) == 16, "FGeometryVertex size must be 16 bytes.");
