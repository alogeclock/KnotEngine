#pragma once

#include "EngineAPI.h"

#include "Core/Math/Matrix.h"
#include "Render/Graph/RenderGraph.h"
#include "Render/RHI/RenderTypes.h"

#include <span>

class FRenderer;
class IRenderDevice;
class FMeshBuffer;
class FMaterialResource;
struct FPrimitiveSceneProxy;
struct FStaticMeshInstance;
class FStaticMeshLODResource;
struct FStaticMeshSceneProxy;
struct FSceneView;
struct FViewConstants;

// 가시 Primitive를 정렬하고 Opaque Pass Node를 현재 프레임의 Render Graph에 추가한다.
class ENGINE_API FOpaquePass
{
public:
	static uint32 AddPass(FRenderGraph& Graph, FRenderer& Renderer, const FSceneView& View, std::span<const FPrimitiveSceneProxy* const> VisiblePrimitives);

private:
	struct FPrimitiveCommand
	{
		const FStaticMeshSceneProxy* Primitive = nullptr;
		const FStaticMeshLODResource* LOD = nullptr;
		const FMeshBuffer* MeshBuffer = nullptr;
		uint64 BatchKey = 0;
	};

	struct FMeshDrawCommand
	{
		const FStaticMeshSceneProxy* Primitive = nullptr;
		const FMeshBuffer* MeshBuffer = nullptr;
		const FMaterialResource* Material = nullptr;
		uint32 FirstIndex = 0;
		uint32 IndexCount = 0;
		uint32 FirstInstance = 0;
		uint32 InstanceCount = 0;
		uint64 SortKey = 0;
		bool bInstanced = false;
	};

	static void ExecutePass(
		FRenderer& Renderer,
		FCommandListHandle CommandList,
		const FRenderViewport& Viewport,
		const FViewConstants& ViewConstants,
		const TArray<FMeshDrawCommand>& OpaqueCommands,
		const TArray<FStaticMeshInstance>& Instances);

	static bool CanBatch(const FPrimitiveCommand& Left, const FPrimitiveCommand& Right);
	static bool CanInstance(const FPrimitiveCommand& Command);
	static uint64 GenerateSortKey(FPipelineStateHandle PipelineState, const FMaterialResource& Material, const FMeshBuffer& MeshBuffer);

	static constexpr uint32 ViewConstantsSlot = 0;
	static constexpr uint32 DrawConstantsSlot = 3;
	static constexpr uint32 MinimumInstanceCount = 4;
};
