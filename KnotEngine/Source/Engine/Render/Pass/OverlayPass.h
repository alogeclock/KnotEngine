#pragma once

#include "EngineAPI.h"

#include "Core/Math/Matrix.h"
#include "Core/Math/Vector4.h"
#include "Render/Graph/RenderGraph.h"
#include "Render/RHI/RenderTypes.h"

#include <span>

class FMeshBuffer;
class IRenderDevice;
class URenderer;
struct FPrimitiveSceneProxy;
struct FSceneView;
struct FShowFlags;
struct FViewConstants;

// Grid, Axis와 Primitive Bounds를 고정 순서로 그리는 단일 Overlay Pass Node다.
class ENGINE_API FOverlayPass
{
public:
	static uint32 AddPass(
		FRenderGraph& Graph,
		URenderer& Renderer,
		const FSceneView& View,
		const FShowFlags& ShowFlags,
		std::span<const FPrimitiveSceneProxy* const> VisiblePrimitives);

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

	struct FBoundsDrawCommand
	{
		FMatrix Model;
	};

	struct alignas(16) FDrawConstants
	{
		FMatrix Model;
	};
	static_assert(sizeof(FDrawConstants) % 16 == 0);

	struct FPassParameters
	{
		FPipelineStateHandle GridPipeline;
		FPipelineStateHandle AxisPipeline;
		FPipelineStateHandle BoundsPipeline;
		const FMeshBuffer* BoundsMeshBuffer = nullptr;
		FGridConstants GridConstants{};
		TArray<FBoundsDrawCommand> BoundsCommands;
	};

	static void ExecutePass(
		IRenderDevice& RenderDevice,
		FCommandListHandle CommandList,
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

	static void DrawBounds(
		IRenderDevice& RenderDevice,
		FCommandListHandle CommandList,
		const FViewConstants& ViewConstants,
		const FPassParameters& Parameters);

	static constexpr uint32 ViewConstantsSlot = 0;
	static constexpr uint32 PassConstantsSlot = 1;
	static constexpr uint32 DrawConstantsSlot = 3;
};
