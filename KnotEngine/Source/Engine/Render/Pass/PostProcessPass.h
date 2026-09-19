#pragma once

#include "EngineAPI.h"

#include "Render/Graph/RenderGraph.h"
#include "Render/RHI/RenderTypes.h"

class IRenderDevice;
class URenderer;

// 선형 Scene Color에 선택 Outline을 합성하고 화면 표시용 sRGB 색으로 변환하는 전체 화면 Post Process Pass다.
class ENGINE_API FPostProcessPass
{
public:
	static uint32 AddPass(
		FRenderGraph& Graph,
		URenderer& Renderer,
		FTextureHandle SceneColor,
		FTextureHandle DisplayColor,
		FTextureHandle SelectionDepth,
		FTextureHandle DepthTarget,
		const FRenderViewport& Viewport);

private:
	static void ExecutePass(
		IRenderDevice& RenderDevice,
		FCommandListHandle CommandList,
		FPipelineStateHandle PipelineState,
		FTextureHandle SceneColor,
		FTextureHandle DisplayColor,
		FTextureHandle SelectionDepth,
		FTextureHandle DepthTarget,
		const FRenderViewport& Viewport);
};
