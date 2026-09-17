#pragma once

#include "EngineAPI.h"

#include "Core/Math/Matrix.h"
#include "Render/Graph/RenderGraph.h"
#include "Render/RHI/RenderTypes.h"
#include "Render/Material/Material.h"

#include <span>

class URenderer;
class IRenderDevice;
class FMeshBuffer;
struct FPrimitiveSceneProxy;
struct FSceneView;
struct FViewConstants;

// 가시 Primitive를 정렬하고 Opaque Pass Node를 현재 프레임의 Render Graph에 추가한다.
class ENGINE_API FOpaquePass
{
public:
	static uint32 AddPass(FRenderGraph& Graph, URenderer& Renderer, const FSceneView& View, std::span<const FPrimitiveSceneProxy* const> VisiblePrimitives);

private:
	struct FMeshDrawCommand
	{
		struct FTextureBinding
		{
			EShaderStage Stage = EShaderStage::Pixel;
			uint32 TextureSlot = 0;
			uint32 SamplerSlot = FSamplerHandle::InvalidIndex;
			FTextureHandle Texture;
			FSamplerHandle Sampler;
		};

		const FPrimitiveSceneProxy* Primitive = nullptr;
		const FMeshBuffer* MeshBuffer = nullptr;
		uint32 FirstIndex = 0;
		uint32 IndexCount = 0;

		FPipelineStateHandle PipelineState;
		TArray<uint8> MaterialConstants;
		TArray<FMaterialConstantBufferBinding> MaterialConstantBuffers;
		TArray<FTextureBinding> Textures;

		uint32 SortKey = 0;
	};

	struct alignas(16) FDrawConstants
	{
		FMatrix Model;
	};
	static_assert(sizeof(FDrawConstants) % 16 == 0);

	static void ExecutePass(
		IRenderDevice& RenderDevice,
		FCommandListHandle CommandList,
		const FRenderViewport& Viewport,
		const FViewConstants& ViewConstants,
		const TArray<FMeshDrawCommand>& OpaqueCommands);

	static constexpr uint32 ViewConstantsSlot = 0;
	static constexpr uint32 DrawConstantsSlot = 3;
};
