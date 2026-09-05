#pragma once

#include "EngineAPI.h"

#include "Core/CoreTypes.h"
#include "Render/RHI/VertexLayout.h"

// 렌더링 백엔드가 공유하는 API 중립 타입.
// - Handle: 네이티브 객체 대신 Index와 Generation으로 RHI 자원을 참조한다.
// - Resource Descriptor: Buffer, Texture 및 Shader 생성에 필요한 속성을 전달한다.
// - Pipeline Descriptor: Shader와 고정 기능 상태 및 출력 대상의 호환 조건을 하나로 묶는다.
// - Command List: 백엔드가 기록 중인 명령 구간을 불투명 Handle로 식별한다.
//
// Descriptor의 기본값은 현재 D3D11 기본 렌더 경로를 나타내며, 백엔드는 지원하지 않는 조합을 생성 시점에 거부한다.

// 네이티브 GPU 객체를 직접 노출하지 않고 슬롯 Index와 Generation으로 참조하는 불투명 Handle이다.

template <typename TagType>
struct TRenderHandle
{
	static constexpr uint32 InvalidIndex = static_cast<uint32>(-1);

	uint32 Index = InvalidIndex;
	uint32 Generation = 0;

	bool IsValid() const { return Index != InvalidIndex; }

	void Reset()
	{
		Index = InvalidIndex;
		Generation = 0;
	}

	bool operator==(const TRenderHandle&) const = default;
};

struct FBufferHandleTag;
struct FTextureHandleTag;
struct FShaderHandleTag;
struct FGraphicsPipelineHandleTag;
struct FCommandListHandleTag;

using FBufferHandle = TRenderHandle<FBufferHandleTag>;
using FTextureHandle = TRenderHandle<FTextureHandleTag>;
using FShaderHandle = TRenderHandle<FShaderHandleTag>;
using FGraphicsPipelineHandle = TRenderHandle<FGraphicsPipelineHandleTag>;
using FCommandListHandle = TRenderHandle<FCommandListHandleTag>;

// Buffer가 GPU Pipeline에서 사용되는 용도를 정의한다.
enum class EBufferUsage : uint8 { Vertex, Index };

// Buffer에 대한 CPU와 GPU의 접근 방식을 정의한다.
enum class EResourceAccess : uint8 { GPUOnly, CPUWrite };

// Buffer 생성에 필요한 크기, 용도 및 접근 방식을 정의한다.
struct ENGINE_API FBufferDesc
{
	uint32 Size = 0;
	EBufferUsage Usage = EBufferUsage::Vertex;
	EResourceAccess Access = EResourceAccess::GPUOnly;
};

// Texture의 Pixel 및 Depth-Stencil 저장 형식을 정의한다.
enum class ETextureFormat : uint8 { RGBA8UNorm, BGRA8UNorm, D24UNormS8UInt };

// Texture가 GPU Pipeline에서 사용되는 용도를 정의한다.
enum class ETextureUsage : uint8 { ShaderResource, RenderTarget, DepthStencil };

// Texture 생성에 필요한 크기, 형식 및 용도를 정의한다.
struct ENGINE_API FTextureDesc
{
	uint32 Width = 0;
	uint32 Height = 0;
	ETextureFormat Format = ETextureFormat::RGBA8UNorm;
	ETextureUsage Usage = ETextureUsage::ShaderResource;
};

// Shader가 실행되는 Graphics Pipeline Stage를 정의한다.
enum class EShaderStage : uint8 { Vertex, Pixel };

// Shader 생성에 필요한 소스 경로, 진입점 및 Stage를 정의한다.
struct ENGINE_API FShaderDesc
{
	FWString SourcePath;
	FString EntryPoint;
	EShaderStage Stage = EShaderStage::Vertex;
};

// 입력 정점을 조립해 Primitive를 구성하는 방식을 정의한다.
enum class EPrimitiveTopology : uint8 { TriangleList };

// Blend 연산에서 Source 및 Destination 색상에 곱할 계수를 정의한다.
enum class EBlendFactor : uint8
{
	Zero,
	One,
	SourceColor,
	InverseSourceColor,
	SourceAlpha,
	InverseSourceAlpha,
	DestinationColor,
	InverseDestinationColor,
	DestinationAlpha,
	InverseDestinationAlpha,
};

// 계수가 적용된 Source 및 Destination 값을 결합하는 연산을 정의한다.
enum class EBlendOperation : uint8 { Add, Subtract, ReverseSubtract, Minimum, Maximum };

// Render Target에 기록할 Color Channel을 Bit Mask로 정의한다.
enum class EColorWriteMask : uint8
{
	None = 0,
	Red = 1 << 0,
	Green = 1 << 1,
	Blue = 1 << 2,
	Alpha = 1 << 3,
	All = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3),
};

// 두 Color Write Mask를 결합한 Mask를 반환한다.
constexpr EColorWriteMask operator|(EColorWriteMask Left, EColorWriteMask Right)
{
	return static_cast<EColorWriteMask>(static_cast<uint8>(Left) | static_cast<uint8>(Right));
}

// 하나의 Render Target에 적용할 Color 및 Alpha Blend 상태를 정의한다.
struct ENGINE_API FRenderTargetBlendDesc
{
	bool bBlendEnabled = false;
	EBlendFactor SourceColorBlend = EBlendFactor::One;
	EBlendFactor DestinationColorBlend = EBlendFactor::Zero;
	EBlendOperation ColorBlendOperation = EBlendOperation::Add;
	EBlendFactor SourceAlphaBlend = EBlendFactor::One;
	EBlendFactor DestinationAlphaBlend = EBlendFactor::Zero;
	EBlendOperation AlphaBlendOperation = EBlendOperation::Add;
	EColorWriteMask ColorWriteMask = EColorWriteMask::All;
};

// Graphics Pipeline의 전체 Blend 상태를 정의한다.
struct ENGINE_API FBlendStateDesc
{
	bool bAlphaToCoverageEnabled = false;
	FRenderTargetBlendDesc RenderTarget;
};

// Rasterizer가 Triangle 내부를 채우는 방식을 정의한다.
enum class EFillMode : uint8 { Solid, Wireframe };

// Rasterizer가 제거할 Triangle 면 방향을 정의한다.
enum class ECullMode : uint8 { None, Front, Back };

// Primitive를 Pixel Fragment로 변환할 때 적용할 Rasterizer 상태를 정의한다.
struct ENGINE_API FRasterizerStateDesc
{
	EFillMode FillMode = EFillMode::Solid;
	ECullMode CullMode = ECullMode::Back;
	bool bFrontCounterClockwise = false;
	int32 DepthBias = 0;
	float DepthBiasClamp = 0.0f;
	float SlopeScaledDepthBias = 0.0f;
	bool bDepthClipEnabled = true;
	bool bMultisampleEnabled = false;
	bool bAntialiasedLineEnabled = false;
};

// Shader와 고정 기능 상태를 하나의 Graphics Pipeline으로 생성하기 위한 계약이다.
struct ENGINE_API FGraphicsPipelineDesc
{
	FShaderHandle VertexShader;
	FShaderHandle PixelShader;
	FVertexLayout VertexLayout;
	EPrimitiveTopology PrimitiveTopology = EPrimitiveTopology::TriangleList;
	bool bDepthTestEnabled = true;
	bool bDepthWriteEnabled = true;
	ETextureFormat RenderTargetFormat = ETextureFormat::BGRA8UNorm; // 현재 Render Target은 1개
	ETextureFormat DepthStencilFormat = ETextureFormat::D24UNormS8UInt;
	uint8 SampleCount = 1;	
	FBlendStateDesc BlendState;
	FRasterizerStateDesc RasterizerState;
};

// Index Buffer의 요소 하나가 사용하는 정수 저장 형식을 정의한다.
enum class EIndexFormat : uint8 { UInt16, UInt32 };

// Render Target에서 Rasterization이 수행될 사각 영역과 Depth 범위를 정의한다.
struct ENGINE_API FRenderViewport
{
	float TopLeftX = 0.0f;
	float TopLeftY = 0.0f;
	float Width = 0.0f;
	float Height = 0.0f;
	float MinDepth = 0.0f;
	float MaxDepth = 1.0f;
};
