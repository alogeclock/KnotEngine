#pragma once

#include "Core/CoreTypes.h"
#include "Render/RHI/VertexLayout.h"

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

enum class EBufferUsage : uint8 { Vertex, Index, Constant };
enum class EResourceAccess : uint8 { GPUOnly, CPUWrite };

struct FBufferDesc
{
	uint32 Size = 0;
	EBufferUsage Usage = EBufferUsage::Vertex;
	EResourceAccess Access = EResourceAccess::GPUOnly;
};

enum class ETextureFormat : uint8 { RGBA8UNorm, BGRA8UNorm, D24UNormS8UInt };
enum class ETextureUsage : uint8 { ShaderResource, RenderTarget, DepthStencil };

struct FTextureDesc
{
	uint32 Width = 0;
	uint32 Height = 0;
	ETextureFormat Format = ETextureFormat::RGBA8UNorm;
	ETextureUsage Usage = ETextureUsage::ShaderResource;
};

enum class EShaderStage : uint8 { Vertex, Pixel };

struct FShaderDesc
{
	FWString SourcePath;
	FString EntryPoint;
	EShaderStage Stage = EShaderStage::Vertex;
};

enum class EPrimitiveTopology : uint8 { TriangleList };

struct FGraphicsPipelineDesc
{
	FShaderHandle VertexShader;
	FShaderHandle PixelShader;
	FVertexLayout VertexLayout;
	EPrimitiveTopology PrimitiveTopology = EPrimitiveTopology::TriangleList;
	bool bDepthTestEnabled = true;
	bool bDepthWriteEnabled = true;
};

enum class EIndexFormat : uint8 { UInt16, UInt32 };

struct FRenderViewport
{
	float TopLeftX = 0.0f;
	float TopLeftY = 0.0f;
	float Width = 0.0f;
	float Height = 0.0f;
	float MinDepth = 0.0f;
	float MaxDepth = 1.0f;
};
