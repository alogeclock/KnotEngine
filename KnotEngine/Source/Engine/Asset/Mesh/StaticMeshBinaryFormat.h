#pragma once

#include "Core/CoreTypes.h"

// Static Mesh .kasset 전체에 한 번 저장되는 고정 크기 헤더다.
struct FStaticMeshBinaryHeader
{
	inline static constexpr char MagicValue[4] = { 'K', 'M', 'S', 'H' };
	inline static constexpr uint32 CurrentVersion = 1;
	inline static constexpr uint32 MaxLODCount = 16;

	char Magic[4];
	uint32 Version;
	uint32 VertexStride;
	uint32 LODCount;
};
static_assert(sizeof(FStaticMeshBinaryHeader) == 16);

// Static Mesh의 각 LOD 데이터 앞에 저장되는 배열 크기다.
struct FStaticMeshLODBinaryHeader
{
	uint32 VertexCount;
	uint32 IndexCount;
};
static_assert(sizeof(FStaticMeshLODBinaryHeader) == 8);
