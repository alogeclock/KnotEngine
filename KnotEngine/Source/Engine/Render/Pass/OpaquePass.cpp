#include "Render/Pass/OpaquePass.h"

#include "Core/Assert.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Render/Renderer.h"
#include "Render/Resource/MeshResources.h"
#include "Render/Resource/MeshTypes.h"
#include "Render/Resource/VertexTypes.h"
#include "Render/RHI/RenderDevice.h"
#include "Render/Scene/SceneView.h"
#include "Source/Resource/resource.h"

#include <algorithm>
#include <bit>
#include <cmath>

uint32 FOpaquePass::AddPass(FRenderGraph& Graph, URenderer& Renderer, const FSceneView& View, std::span<const FPrimitiveSceneProxy* const> VisiblePrimitives)
{
	IRenderDevice* RenderDevice = &Renderer.GetRenderDevice();

	const FCommandListHandle CommandList = Renderer.GetCommandList();
	check(CommandList.IsValid());

	check(GShaderRegistry && GPipelineStateCache);
	const FShaderHandle VertexShader = GShaderRegistry->GetOrCreate({ IDR_COMMON_SHADER, "Common.hlsl", "VS", EShaderStage::Vertex });
	const FShaderHandle PixelShader = GShaderRegistry->GetOrCreate({ IDR_COMMON_SHADER, "Common.hlsl", "PS", EShaderStage::Pixel });

	FPipelineStateDesc PipelineStateDesc;
	PipelineStateDesc.VertexShader = VertexShader;
	PipelineStateDesc.PixelShader = PixelShader;
	PipelineStateDesc.VertexLayout = FGeometryVertex::GetVertexLayout();
	const FPipelineStateHandle PipelineState = GPipelineStateCache->GetOrCreate(PipelineStateDesc);

	TArray<FMeshDrawCommand> OpaqueCommands;
	OpaqueCommands.reserve(VisiblePrimitives.size());

	for (const FPrimitiveSceneProxy* Primitive : VisiblePrimitives)
	{
		const float Depth = View.ViewMatrix.TransformPosition(Primitive->WorldBounds.GetCenter()).Z;
		check(!std::isnan(Depth) && !std::isinf(Depth));
		const uint32 SortKey = std::bit_cast<uint32>(std::max(0.0f, Depth));
		OpaqueCommands.push_back({ Primitive, SortKey });
	}

	std::stable_sort(OpaqueCommands.begin(), OpaqueCommands.end(), [](const FMeshDrawCommand& Left, const FMeshDrawCommand& Right)
	{
		return Left.SortKey < Right.SortKey;
	});

	const FViewConstants ViewConstants = {
		View.ViewProjectionMatrix,
		View.ViewProjectionMatrix.GetInverse(),
		View.ViewOrigin,
		View.FarClip,
	};
	const FRenderViewport Viewport = View.Viewport;
	return Graph.AddPass("Opaque", [RenderDevice, CommandList, PipelineState, Viewport, ViewConstants, OpaqueCommands = std::move(OpaqueCommands)]()
	{
		ExecutePass(*RenderDevice, CommandList, PipelineState, Viewport, ViewConstants, OpaqueCommands);
	});
}

void FOpaquePass::ExecutePass(
	IRenderDevice& RenderDevice,
	FCommandListHandle CommandList,
	FPipelineStateHandle PipelineState,
	const FRenderViewport& Viewport,
	const FViewConstants& ViewConstants,
	const TArray<FMeshDrawCommand>& OpaqueCommands)
{
	RenderDevice.SetViewport(CommandList, Viewport);
	RenderDevice.SetPipelineState(CommandList, PipelineState);
	const auto* ViewBytes = reinterpret_cast<const uint8*>(&ViewConstants);
	RenderDevice.SetConstantData(CommandList, EShaderStage::Vertex, ViewConstantsSlot, std::span<const uint8>(ViewBytes, sizeof(ViewConstants)));

	for (const FMeshDrawCommand& Command : OpaqueCommands)
	{
		const FMeshBuffer& MeshBuffer = *Command.Primitive->Mesh->GetMeshBuffer();
		checkf(MeshBuffer.IsValid(), "유효하지 않은 FMeshBuffer가 Opaque Pass에 전달되었다.");
		checkf(MeshBuffer.GetLayout() == FGeometryVertex::GetVertexLayout(), "Opaque Pipeline State와 호환되지 않는 Vertex Layout이다.");

		const FDrawConstants DrawConstants = { Command.Primitive->WorldMatrix };
		const auto* DrawBytes = reinterpret_cast<const uint8*>(&DrawConstants);
		RenderDevice.SetConstantData(CommandList, EShaderStage::Vertex, DrawConstantsSlot, std::span<const uint8>(DrawBytes, sizeof(DrawConstants)));
		RenderDevice.SetVertexBuffer(CommandList, MeshBuffer.GetVertexBuffer().GetHandle(), MeshBuffer.GetStride());
		if (MeshBuffer.GetIndexCount() > 0)
		{
			RenderDevice.SetIndexBuffer(CommandList, MeshBuffer.GetIndexBuffer().GetHandle(), EIndexFormat::UInt32);
			RenderDevice.DrawIndexed(CommandList, MeshBuffer.GetIndexCount());
		}
		else
		{
			RenderDevice.Draw(CommandList, MeshBuffer.GetVertexCount());
		}
	}
}
