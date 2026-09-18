#pragma once

#include "EngineAPI.h"

#include "Core/CoreTypes.h"
#include "Core/Name.h"
#include "Render/RHI/VertexLayout.h"

#include <limits>
#include <span>

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
struct FSamplerHandleTag;
struct FShaderHandleTag;
struct FPipelineStateHandleTag;
struct FCommandListHandleTag;

using FBufferHandle = TRenderHandle<FBufferHandleTag>;
using FTextureHandle = TRenderHandle<FTextureHandleTag>;
using FSamplerHandle = TRenderHandle<FSamplerHandleTag>;
using FShaderHandle = TRenderHandle<FShaderHandleTag>;
using FPipelineStateHandle = TRenderHandle<FPipelineStateHandleTag>;
using FCommandListHandle = TRenderHandle<FCommandListHandleTag>;

// Buffer가 GPU Pipeline에서 사용되는 용도를 정의한다.
enum class EBufferUsage : uint8
{
	Vertex,
	Index
};

// Buffer에 대한 CPU와 GPU의 접근 방식을 정의한다.
enum class EResourceAccess : uint8
{
	GPUOnly,
	CPUWrite
};

// Buffer 생성에 필요한 크기, 용도 및 접근 방식을 정의한다.
struct ENGINE_API FBufferDesc
{
	uint32 Size = 0;
	EBufferUsage Usage = EBufferUsage::Vertex;
	EResourceAccess Access = EResourceAccess::GPUOnly;
};

// Texture의 Pixel 및 Depth-Stencil 저장 형식을 정의한다.
enum class ETextureFormat : uint8
{
	R8UNorm,
	RG8UNorm,
	RGBA8UNorm,
	BGRA8UNorm,
	BC1UNorm,
	BC3UNorm,
	BC5UNorm,
	BC7UNorm,
	D24UNormS8UInt,
	D32Float,
};

// Texture가 GPU Pipeline에서 사용되는 용도를 정의한다.
enum class ETextureUsage : uint8
{
	None = 0,
	ShaderResource = 1 << 0,
	RenderTarget = 1 << 1,
	DepthStencil = 1 << 2,
};

constexpr ETextureUsage operator|(ETextureUsage Left, ETextureUsage Right)
{
	return static_cast<ETextureUsage>(static_cast<uint8>(Left) | static_cast<uint8>(Right));
}

constexpr bool HasAnyTextureUsage(ETextureUsage Value, ETextureUsage Flags)
{
	return (static_cast<uint8>(Value) & static_cast<uint8>(Flags)) != 0;
}

// Texture 생성에 필요한 크기, 형식 및 용도를 정의한다.
struct ENGINE_API FTextureDesc
{
	uint32 Width = 0;
	uint32 Height = 0;
	uint32 MipCount = 1;
	ETextureFormat Format = ETextureFormat::RGBA8UNorm;
	ETextureUsage Usage = ETextureUsage::ShaderResource;
	bool bSRGB = false;
};

// Texture Mip 하나의 초기 데이터와 메모리 행 간격을 전달하는 비소유 업로드 뷰.
struct ENGINE_API FTextureSubresourceData
{
	std::span<const uint8> Data;
	uint32 RowPitch = 0;
	uint32 SlicePitch = 0;
};

// Texture를 Sample할 때 적용할 보간 방식을 정의한다.
enum class ESamplerFilter : uint8
{
	Point,
	Bilinear,
	Trilinear,
	Anisotropic
};

// Texture 좌표가 0~1 범위를 벗어났을 때 적용할 주소 지정 방식을 정의한다.
enum class ESamplerAddressMode : uint8
{
	Wrap,
	Mirror,
	Clamp,
	Border
};

// Shader에 바인딩할 Sampler 상태를 정의한다.
struct ENGINE_API FSamplerDesc
{
	ESamplerFilter Filter = ESamplerFilter::Trilinear;
	ESamplerAddressMode AddressU = ESamplerAddressMode::Wrap;
	ESamplerAddressMode AddressV = ESamplerAddressMode::Wrap;
	ESamplerAddressMode AddressW = ESamplerAddressMode::Wrap;
	float MipLODBias = 0.0f;
	uint32 MaxAnisotropy = 1;
	float BorderColor[4] = {};
	float MinLOD = 0.0f;
	float MaxLOD = (std::numeric_limits<float>::max)();

	bool operator==(const FSamplerDesc&) const = default;
};

// Shader가 실행되는 Pipeline Stage를 정의한다.
enum class EShaderStage : uint8
{
	Vertex,
	Pixel
};

// Shader Reflection에서 Constant 변수가 사용하는 HLSL 기본 자료형이다.
enum class EShaderParameterBaseType : uint8
{
	Unknown,
	Float,
	Int,
	UInt,
	Bool
};

// Shader Reflection에서 Constant 변수의 Scalar, Vector, Matrix 형태를 구분한다.
enum class EShaderParameterClass : uint8
{
	Unknown,
	Scalar,
	Vector,
	MatrixRows,
	MatrixColumns,
	Struct
};

// Shader Constant Buffer 안에 배치된 변수 하나의 컴파일 결과다.
struct ENGINE_API FShaderParameterDesc
{
	FName Name;
	EShaderParameterBaseType BaseType = EShaderParameterBaseType::Unknown;
	EShaderParameterClass Class = EShaderParameterClass::Unknown;
	uint32 Offset = 0;
	uint32 Size = 0;
	uint32 Rows = 0;
	uint32 Columns = 0;
	uint32 Elements = 0;
};

// Shader Stage의 b 슬롯에 바인딩되는 Constant Buffer와 변수 배치를 나타낸다.
struct ENGINE_API FShaderConstantBufferDesc
{
	FName Name;
	EShaderStage Stage = EShaderStage::Vertex;
	uint32 Slot = 0;
	uint32 Size = 0;
	TArray<FShaderParameterDesc> Parameters;
};

// Shader Stage에서 사용하는 Texture 또는 Sampler 자원 종류다.
enum class EShaderResourceType : uint8
{
	Unknown,
	Texture2D,
	TextureCube,
	Sampler
};

// 컴파일된 Shader 자원의 이름과 Stage별 Register 범위를 나타낸다.
struct ENGINE_API FShaderResourceBindingDesc
{
	FName Name;
	EShaderStage Stage = EShaderStage::Vertex;
	EShaderResourceType Type = EShaderResourceType::Unknown;
	uint32 Slot = 0;
	uint32 Count = 1;
};

// D3DReflect 결과를 API 중립 형태로 보관하는 컴파일된 Shader 메타데이터다.
struct ENGINE_API FShaderReflection
{
	TArray<FShaderConstantBufferDesc> ConstantBuffers;
	TArray<FShaderResourceBindingDesc> Resources;
};

// 이미 컴파일된 Shader Bytecode와 Stage를 RHI에 전달한다.
struct ENGINE_API FShaderBytecodeDesc
{
	std::span<const uint8> Bytecode;
	FString DebugName;
	EShaderStage Stage = EShaderStage::Vertex;
};

// 입력 정점을 조립해 Primitive를 구성하는 방식을 정의한다.
enum class EPrimitiveTopology : uint8
{
	TriangleList,
	LineList
};

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
enum class EBlendOperation : uint8
{
	Add,
	Subtract,
	ReverseSubtract,
	Minimum,
	Maximum
};

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

	bool operator==(const FRenderTargetBlendDesc&) const = default;
};

// Pipeline State의 전체 Blend 상태를 정의한다.
struct ENGINE_API FBlendStateDesc
{
	bool bAlphaToCoverageEnabled = false;
	FRenderTargetBlendDesc RenderTarget;

	bool operator==(const FBlendStateDesc&) const = default;
};

// Rasterizer가 Triangle 내부를 채우는 방식을 정의한다.
enum class EFillMode : uint8
{
	Solid,
	Wireframe
};

// Rasterizer가 제거할 Triangle 면 방향을 정의한다.
enum class ECullMode : uint8
{
	None,
	Front,
	Back
};

// Graphics Pass가 Depth Buffer를 검사하고 갱신하는 공용 정책이다.
enum class EDepthMode : uint8
{
	ReadWrite,
	ReadOnly,
	Disabled,
};

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

	bool operator==(const FRasterizerStateDesc&) const = default;
};

// Shader와 고정 기능 상태를 하나의 Pipeline State로 생성하기 위한 계약이다.
struct ENGINE_API FPipelineStateDesc
{
	FShaderHandle VertexShader;
	FShaderHandle PixelShader;
	FVertexLayout VertexLayout;
	EPrimitiveTopology PrimitiveTopology = EPrimitiveTopology::TriangleList;
	EDepthMode DepthMode = EDepthMode::ReadWrite;
	ETextureFormat RenderTargetFormat = ETextureFormat::BGRA8UNorm; // 현재 Render Target은 1개
	ETextureFormat DepthStencilFormat = ETextureFormat::D32Float;
	uint8 SampleCount = 1;
	FBlendStateDesc BlendState;
	FRasterizerStateDesc RasterizerState;

	bool operator==(const FPipelineStateDesc&) const = default;
};

// Index Buffer의 요소 하나가 사용하는 정수 저장 형식을 정의한다.
enum class EIndexFormat : uint8
{
	UInt16,
	UInt32
};

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
