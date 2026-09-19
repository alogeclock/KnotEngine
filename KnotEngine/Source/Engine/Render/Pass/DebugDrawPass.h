#pragma once

#include "EngineAPI.h"

#include "Render/DebugDraw/DebugDraw.h"
#include "Render/Graph/RenderGraph.h"

#include <span>

class IRenderDevice;
class URenderer;
struct FPrimitiveSceneProxy;
struct FSceneView;
struct FViewConstants;

// 수집된 Debug 선은 동적 Line Batch로, 기본 도형은 Indexed Instance Batch로 렌더링한다.
class ENGINE_API FDebugDrawPass final
{
public:
	static uint32 AddPass(
		FRenderGraph& Graph,
		URenderer& Renderer,
		const FSceneView& View,
		std::span<const FPrimitiveSceneProxy* const> BoundsPrimitives,
		FTextureHandle ColorTarget,
		FTextureHandle DepthTarget);

private:
	struct FShapeBatch
	{
		const FMeshBuffer* MeshBuffer = nullptr;
		TArray<FDebugDraw::FInstance> Instances;
	};

	struct FPassParameters
	{
		FPipelineStateHandle LinePipeline;
		FPipelineStateHandle InstancePipeline;
		FBufferHandle LineBuffer;
		uint32 LineVertexCount = 0;
		TArray<FShapeBatch> ShapeBatches;
	};

	static void ExecutePass(
		IRenderDevice& RenderDevice,
		FCommandListHandle CommandList,
		FTextureHandle ColorTarget,
		FTextureHandle DepthTarget,
		const FRenderViewport& Viewport,
		const FViewConstants& ViewConstants,
		const FPassParameters& Parameters);
	static void DrawLines(
		IRenderDevice& RenderDevice,
		FCommandListHandle CommandList,
		const FViewConstants& ViewConstants,
		const FPassParameters& Parameters);
	static void DrawShapes(
		IRenderDevice& RenderDevice,
		FCommandListHandle CommandList,
		const FViewConstants& ViewConstants,
		const FPassParameters& Parameters);

	static constexpr uint32 ViewConstantsSlot = 0;
	static constexpr uint32 InstanceConstantsSlot = 3;
	static constexpr uint32 MaxInstancesPerDraw = 128;
};
