#pragma once

#include "Core/CoreTypes.h"

// GPU Shader 관점에서 Vertex Data의 의미를 정의하는 열거형
enum class FVertexSemantic : uint8
{
    Position,
    Normal,
    Tangent,
    Color,
    TexCoord0,
};

// 메모리 관점에서 Vertex Data의 저장 형식을 정의하는 열거형
enum class FVertexFormat : uint8
{
    Float1,
    Float2,
    Float3,
    Float4,

    Half2,
    Half4,

    UInt8x4,
    UNorm8x4,
    SNorm8x4,

    UInt16x2,
    UInt16x4,
    UNorm16x2,
    UNorm16x4,
    SNorm16x2,
    SNorm16x4,

    UInt32,
    UInt32x2,
    UInt32x3,
    UInt32x4,
};

// Vertex Buffer 안에 들어있는 정점 attribute의 의미, 형식, 오프셋을 정의하는 구조체
struct FVertexElement
{
    FVertexSemantic Semantic;
    FVertexFormat Format;
    uint8 SemanticIndex; // 동일한 Semantic이 여러 개 있을 경우 구분하기 위한 인덱스
    uint8 StreamIndex;   // Non-interleaved Vertex Buffer를 지원하기 위한 Vertex Stream 인덱스 (SoA 구조)
    uint16 Offset;
};

// Vertex Buffer 안에 들어있는 정점 attribute들의 집합을 정의하는 구조체
struct FVertexLayout
{
	const TArray<FVertexElement> Elements;
	uint32 ElementCount;
    uint32 Stride; // Vertex 한 개의 크기 (바이트 단위)
};