#pragma once

#include "EngineAPI.h"

#include "Core/Math/Vector2.h"
#include "Core/Math/Vector.h"
#include "Core/Math/Matrix.h"
#include "Render/RHI/VertexLayout.h"

#include <type_traits>

constexpr uint32 PackRGBA(uint8 R, uint8 G, uint8 B, uint8 A = 255)
{
	return uint32(R) | (uint32(G) << 8) | (uint32(B) << 16) | (uint32(A) << 24);
}

// 선, 큐브, 삼각형 등 단순 디버그용 Geometry를 그리기 위한 Vertex 구조체
struct ENGINE_API FGeometryVertex
{
	FVector Position;
	uint32 Color = 0;

	static const FVertexLayout& GetVertexLayout();
};
static_assert(std::is_standard_layout_v<FGeometryVertex>, "FGeometryVertex must have a standard layout.");
static_assert(std::is_trivially_copyable_v<FGeometryVertex>, "FGeometryVertex must be trivially copyable.");
static_assert(sizeof(FGeometryVertex) == 16, "FGeometryVertex size must be 16 bytes.");

// Static Mesh의 기본 Vertex 입력. Tangent.xyz와 Handedness를 TANGENT.xyzw로 전달한다.
// Bitangent = (Normal × Tangent) * Handedness로 복원한다. 
// +1은 Normal × Tangent 방향, -1은 그 반대 방향이며 Mirrored UV의 Tangent Space를 표현한다.
struct ENGINE_API FStaticMeshVertex
{
	FVector Position;
	FVector Normal;
	FVector Tangent;
	float Handedness = 1.0f;
	FVector2 TexCoord;

	static const FVertexLayout& GetVertexLayout();
};
static_assert(std::is_standard_layout_v<FStaticMeshVertex>, "FStaticMeshVertex must have a standard layout.");
static_assert(std::is_trivially_copyable_v<FStaticMeshVertex>, "FStaticMeshVertex must be trivially copyable.");
static_assert(sizeof(FStaticMeshVertex) == 48, "FStaticMeshVertex size must be 48 bytes.");

// 같은 Static Mesh Draw가 공유하는 객체별 World Transform이다.
struct ENGINE_API FStaticMeshInstance
{
	FMatrix WorldMatrix;

	static const FVertexLayout& GetVertexLayout();
};
static_assert(std::is_standard_layout_v<FStaticMeshInstance>, "FStaticMeshInstance must have a standard layout.");
static_assert(std::is_trivially_copyable_v<FStaticMeshInstance>, "FStaticMeshInstance must be trivially copyable.");
static_assert(sizeof(FStaticMeshInstance) == 64, "FStaticMeshInstance size must be 64 bytes.");
