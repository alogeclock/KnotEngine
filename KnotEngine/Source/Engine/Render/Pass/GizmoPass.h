#pragma once

#include "EngineAPI.h"

#include "Core/Math/Vector.h"
#include "Render/Graph/RenderGraph.h"
#include "Render/RHI/RenderTypes.h"

class FRenderer;
class IRenderDevice;
struct FMatrix;
struct FSceneView;

// View에 복사된 Transform Gizmo 상태를 Scene 위의 Overlay로 그린다.
class ENGINE_API FGizmoPass
{
public:
	static uint32 AddPass(FRenderGraph& Graph, FRenderer& Renderer, const FSceneView& View, FTextureHandle ColorTarget, FTextureHandle DepthTarget);

private:
	struct alignas(16) FGizmoConstants
	{
		FVector Origin;
		float WorldScale = 1.0f;
		FVector ViewRight;
		uint32 Mode = 0;
		FVector ViewUp;
		int32 HighlightedAxis = -1;
		FVector ViewOrigin;
		uint32 Padding = 0;
		float ViewportWidth = 1.0f;
		float ViewportHeight = 1.0f;
		float Padding2[2] = {};
	};
	static_assert(sizeof(FGizmoConstants) == 80);

	static void ExecutePass(
		IRenderDevice& RenderDevice,
		FCommandListHandle CommandList,
		FTextureHandle ColorTarget,
		FTextureHandle DepthTarget,
		const FRenderViewport& Viewport,
		const FMatrix& ViewProjection,
		FPipelineStateHandle Pipeline,
		const FGizmoConstants& Constants,
		uint32 VertexCount);
};
