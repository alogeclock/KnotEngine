#pragma once

#include "EngineAPI.h"

#include "Core/Math/Matrix.h"
#include "Render/Graph/RenderGraph.h"
#include "Render/RHI/RenderTypes.h"

#include <span>

class FMeshBuffer;
class IRenderDevice;
class FRenderer;
struct FPrimitiveSceneProxy;
struct FSceneView;
struct FViewConstants;

// 선택된 Primitive의 전체 실루엣 Depth를 별도 Target에 기록한다.
class ENGINE_API FSelectionPass
{
public:
	static uint32 AddPass(
		FRenderGraph& Graph,
		FRenderer& Renderer,
		const FSceneView& View,
		std::span<const FPrimitiveSceneProxy* const> VisiblePrimitives,
		FTextureHandle SelectionDepth);

private:
	struct FSelectionDrawCommand
	{
		const FPrimitiveSceneProxy* Primitive = nullptr;
		const FMeshBuffer* MeshBuffer = nullptr;
		uint32 FirstIndex = 0;
		uint32 IndexCount = 0;
	};

	struct alignas(16) FDrawConstants
	{
		FMatrix Model;
	};
	static_assert(sizeof(FDrawConstants) % 16 == 0);

	static void ExecutePass(
		IRenderDevice& RenderDevice,
		FCommandListHandle CommandList,
		FPipelineStateHandle PipelineState,
		FTextureHandle SelectionDepth,
		const FRenderViewport& Viewport,
		const FViewConstants& ViewConstants,
		const TArray<FSelectionDrawCommand>& DrawCommands);

	static constexpr uint32 ViewConstantsSlot = 0;
	static constexpr uint32 DrawConstantsSlot = 3;
};
