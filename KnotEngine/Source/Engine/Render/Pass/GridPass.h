#pragma once

#include "EngineAPI.h"

#include "Core/Math/Matrix.h"
#include "Core/Math/Vector4.h"
#include "Render/Graph/RenderGraph.h"
#include "Render/RHI/RenderTypes.h"

class URenderer;
class IRenderDevice;
struct FSceneView;
struct FViewConstants;

// Pixel Shader 기반 Grid Pass Node를 현재 프레임의 Render Graph에 추가한다.
class ENGINE_API FGridPass
{
public:
	static uint32 AddPass(FRenderGraph& Graph, URenderer& Renderer, const FSceneView& View);

private:
	struct alignas(16) FGridConstants
	{
		float GridSpacing;
		float MajorGridInterval;
		float Padding0;
		float Padding1;
		FVector4 MinorColor;
		FVector4 MajorColor;
	};
	static_assert(sizeof(FGridConstants) == 48);

	static void ExecutePass(
		IRenderDevice& RenderDevice,
		FCommandListHandle CommandList,
		FPipelineStateHandle PipelineState,
		const FRenderViewport& Viewport,
		const FViewConstants& ViewConstants,
		const FGridConstants& GridConstants);

	static constexpr uint32 ViewConstantsSlot = 0;
	static constexpr uint32 GridConstantsSlot = 1;
};
