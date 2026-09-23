#pragma once

#include "EngineAPI.h"

#include "Core/Math/Matrix.h"
#include "Core/Math/Vector2.h"
#include "Core/Math/Vector4.h"
#include "Render/Graph/RenderGraph.h"
#include "Render/RHI/RenderTypes.h"

class IRenderDevice;
class FRenderer;
struct FSceneView;
struct FShowFlags;
struct FViewConstants;

// Grid와 World Axis를 고정 순서로 그리는 Overlay Pass Node다.
class ENGINE_API FOverlayPass
{
public:
	static uint32 AddPass(
		FRenderGraph& Graph,
		FRenderer& Renderer,
		const FSceneView& View,
		const FShowFlags& ShowFlags,
		FTextureHandle ColorTarget,
		FTextureHandle DepthTarget,
		FTextureHandle SelectionDepth,
		bool bHasSelection);

private:
	struct alignas(16) FOverlayConstants
	{
		uint32 HasSelection;
		float GridSpacing;
		float MajorGridInterval;
		uint32 Padding;
		FVector4 MinorColor;
		FVector4 MajorColor;
		FMatrix Projection;
		FMatrix InverseProjection;
		FMatrix InverseViewRotation;
		FVector2 GridOriginPhase;
		float CameraHeight;
		float Padding2;
	};
	static_assert(sizeof(FOverlayConstants) == 256);

	struct FPassParameters
	{
		FPipelineStateHandle GridPipeline;
		FPipelineStateHandle AxisPipeline;
		FOverlayConstants OverlayConstants{};
	};

	static void ExecutePass(
		IRenderDevice& RenderDevice,
		FCommandListHandle CommandList,
		FTextureHandle ColorTarget,
		FTextureHandle DepthTarget,
		FTextureHandle SelectionDepth,
		const FRenderViewport& Viewport,
		const FViewConstants& ViewConstants,
		const FPassParameters& Parameters);

	static void DrawGrid(
		IRenderDevice& RenderDevice,
		FCommandListHandle CommandList,
		const FViewConstants& ViewConstants,
		const FPassParameters& Parameters);

	static void DrawAxis(
		IRenderDevice& RenderDevice,
		FCommandListHandle CommandList,
		const FViewConstants& ViewConstants,
		const FPassParameters& Parameters);

	static constexpr uint32 ViewConstantsSlot = 0;
	static constexpr uint32 OverlayConstantsSlot = 1;
};
