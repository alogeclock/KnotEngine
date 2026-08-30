#pragma once

#include "Core/CoreTypes.h"

// Shader 입력에서 정점 Attribute가 갖는 의미를 API와 무관하게 표현한다.
enum class EVertexSemantic : uint8
{
	Position,
	Normal,
	Tangent,
	Color,
	TexCoord0,
};

// 정점 Attribute의 메모리 저장 형식이다. 각 Render Device가 네이티브 포맷으로 변환한다.
enum class EVertexFormat : uint8
{
	Float1, Float2, Float3, Float4,
	Half2, Half4,
	UInt8x4, UNorm8x4, SNorm8x4,
	UInt16x2, UInt16x4, UNorm16x2, UNorm16x4, SNorm16x2, SNorm16x4,
	UInt32, UInt32x2, UInt32x3, UInt32x4,
};

// 하나의 정점 Attribute가 사용하는 Semantic, Format 및 Vertex 내 Byte Offset을 정의한다.
struct FVertexElement
{
	EVertexSemantic Semantic;
	EVertexFormat Format;
	uint8 SemanticIndex;
	uint16 Offset;

	bool operator==(const FVertexElement&) const = default;
};

// Graphics Pipeline과 Vertex Buffer가 공유하는 정점 입력 계약이다.
struct FVertexLayout
{
	TArray<FVertexElement> Elements;
	uint16 Stride = 0;

	bool operator==(const FVertexLayout&) const = default;
};

// Vertex Layout 검증과 Buffer 크기 계산에 사용할 Format별 Byte 크기를 반환한다.
constexpr uint16 GetVertexFormatBytes(EVertexFormat Format)
{
	switch (Format)
	{
	case EVertexFormat::Float1:
	case EVertexFormat::UInt32:
	case EVertexFormat::Half2:
	case EVertexFormat::UInt8x4:
	case EVertexFormat::UNorm8x4:
	case EVertexFormat::SNorm8x4:
	case EVertexFormat::UInt16x2:
	case EVertexFormat::UNorm16x2:
	case EVertexFormat::SNorm16x2:
		return 4;
	case EVertexFormat::Float2:
	case EVertexFormat::Half4:
	case EVertexFormat::UInt16x4:
	case EVertexFormat::UNorm16x4:
	case EVertexFormat::SNorm16x4:
	case EVertexFormat::UInt32x2:
		return 8;
	case EVertexFormat::Float3:
	case EVertexFormat::UInt32x3:
		return 12;
	case EVertexFormat::Float4:
	case EVertexFormat::UInt32x4:
		return 16;
	}
	return 0;
}
