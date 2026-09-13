#pragma once

#include "EngineAPI.h"

#include "Render/Graph/RenderGraph.h"
#include "Render/RHI/RenderTypes.h"

class URenderer;
class IRenderDevice;
struct FSceneView;
struct FViewConstants;

// Pixel Shader 기반 Axis Pass Node를 현재 프레임의 Render Graph에 추가한다.
class ENGINE_API FAxisPass
{
public:
	static uint32 AddPass(FRenderGraph& Graph, URenderer& Renderer, const FSceneView& View);

private:
	static void ExecutePass(
		IRenderDevice& RenderDevice,
		FCommandListHandle CommandList,
		FPipelineStateHandle PipelineState,
		const FRenderViewport& Viewport,
		const FViewConstants& ViewConstants);

	static constexpr uint32 ViewConstantsSlot = 0;
};
