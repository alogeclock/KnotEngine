#pragma once

// D3D11/D3D12 백엔드가 공유하는 변환 헬퍼.
// DXGI 포맷과 시맨틱 이름은 API 버전과 무관하게 동일하므로 백엔드마다 중복해서 두지 않는다.

#include "Render/RHI/VertexLayout.h"

#include <d3dcommon.h>
#include <dxgiformat.h>

constexpr const char* GetSemanticName(EVertexSemantic Semantic)
{
	switch (Semantic)
	{
	case EVertexSemantic::Position: return "POSITION";
	case EVertexSemantic::Normal: return "NORMAL";
	case EVertexSemantic::Tangent: return "TANGENT";
	case EVertexSemantic::Color: return "COLOR";
	case EVertexSemantic::TexCoord0: return "TEXCOORD";
	}
	return nullptr;
}

constexpr DXGI_FORMAT GetDXGIFormat(EVertexFormat Format)
{
	switch (Format)
	{
	case EVertexFormat::Float1: return DXGI_FORMAT_R32_FLOAT;
	case EVertexFormat::Float2: return DXGI_FORMAT_R32G32_FLOAT;
	case EVertexFormat::Float3: return DXGI_FORMAT_R32G32B32_FLOAT;
	case EVertexFormat::Float4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
	case EVertexFormat::Half2: return DXGI_FORMAT_R16G16_FLOAT;
	case EVertexFormat::Half4: return DXGI_FORMAT_R16G16B16A16_FLOAT;
	case EVertexFormat::UInt8x4: return DXGI_FORMAT_R8G8B8A8_UINT;
	case EVertexFormat::UNorm8x4: return DXGI_FORMAT_R8G8B8A8_UNORM;
	case EVertexFormat::SNorm8x4: return DXGI_FORMAT_R8G8B8A8_SNORM;
	case EVertexFormat::UInt16x2: return DXGI_FORMAT_R16G16_UINT;
	case EVertexFormat::UInt16x4: return DXGI_FORMAT_R16G16B16A16_UINT;
	case EVertexFormat::UNorm16x2: return DXGI_FORMAT_R16G16_UNORM;
	case EVertexFormat::UNorm16x4: return DXGI_FORMAT_R16G16B16A16_UNORM;
	case EVertexFormat::SNorm16x2: return DXGI_FORMAT_R16G16_SNORM;
	case EVertexFormat::SNorm16x4: return DXGI_FORMAT_R16G16B16A16_SNORM;
	case EVertexFormat::UInt32: return DXGI_FORMAT_R32_UINT;
	case EVertexFormat::UInt32x2: return DXGI_FORMAT_R32G32_UINT;
	case EVertexFormat::UInt32x3: return DXGI_FORMAT_R32G32B32_UINT;
	case EVertexFormat::UInt32x4: return DXGI_FORMAT_R32G32B32A32_UINT;
	}
	return DXGI_FORMAT_UNKNOWN;
}

// 셰이더 컴파일 실패 시 D3DCompiler가 남긴 오류 문자열. 파일 자체를 못 열면 Blob이 비어 있다.
inline const char* GetShaderError(ID3DBlob* ErrorBlob)
{
	return ErrorBlob ? static_cast<const char*>(ErrorBlob->GetBufferPointer()) : "(컴파일러 출력 없음 - 셰이더 파일 경로를 확인할 것)";
}
