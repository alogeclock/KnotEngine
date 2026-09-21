#include "Render/Pass/SelectionPass.h"

#include "Core/Assert.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Render/Renderer.h"
#include "Asset/Mesh/StaticMesh.h"
#include "Render/Resource/Mesh/MeshBuffer.h"
#include "Render/Resource/Mesh/StaticMeshResource.h"
#include "Render/Resource/Mesh/Vertex.h"
#include "Render/RHI/RenderDevice.h"
#include "Render/Scene/SceneView.h"

// 선택된 Static Mesh Section을 수집해 Selection Depth Pass를 Render Graph에 추가한다.
uint32 FSelectionPass::AddPass(
	FRenderGraph& Graph,
	FRenderer& Renderer,
	const FSceneView& View,
	std::span<const FPrimitiveSceneProxy* const> VisiblePrimitives,
	FTextureHandle SelectionDepth)
{
	check(SelectionDepth.IsValid());
	TArray<FSelectionDrawCommand> DrawCommands;
	for (const FPrimitiveSceneProxy* Primitive : VisiblePrimitives)
	{
		if (!Primitive->bSelected)
		{
			continue;
		}

		const auto& StaticMeshProxy = static_cast<const FStaticMeshSceneProxy&>(*Primitive);
		check(StaticMeshProxy.MeshResource && StaticMeshProxy.MeshResource->GetLODCount() > 0);
		const SIZE_T LODIndex = StaticMeshProxy.SelectLOD(View);
		const FStaticMeshLODResource& LOD = StaticMeshProxy.MeshResource->GetLOD(LODIndex);
		const FMeshBuffer* MeshBuffer = &LOD.GetMeshBuffer();
		checkf(MeshBuffer->IsValid(), "준비되지 않은 Static Mesh가 Selection Pass에 전달되었다.");
		for (const FStaticMeshSectionResource& Section : LOD.GetSections())
		{
			DrawCommands.push_back({ Primitive, MeshBuffer, Section.FirstIndex, Section.IndexCount });
		}
	}

	if (DrawCommands.empty())
	{
		return FRenderGraph::InvalidIndex;
	}

	const FCommandListHandle CommandList = Renderer.GetCommandList();
	check(CommandList.IsValid());
	FShaderRegistry& ShaderRegistry = Renderer.GetShaderRegistry();
	FPipelineStateDesc PipelineStateDesc;
	PipelineStateDesc.VertexShader = ShaderRegistry.GetOrCreate({ "/Engine/Shader/Selection.hlsl", "VS", EShaderStage::Vertex });
	PipelineStateDesc.PixelShader = ShaderRegistry.GetOrCreate({ "/Engine/Shader/Selection.hlsl", "PS", EShaderStage::Pixel });
	PipelineStateDesc.VertexLayout = FStaticMeshVertex::GetVertexLayout();
	PipelineStateDesc.DepthMode = EDepthMode::ReadWrite;
	PipelineStateDesc.RasterizerState.CullMode = ECullMode::None;
	const FPipelineStateHandle PipelineState = Renderer.GetPipelineStateCache().GetOrCreate(PipelineStateDesc);
	IRenderDevice* RenderDevice = &Renderer.GetRenderDevice();
	const FViewConstants ViewConstants = {
		View.ViewProjectionMatrix,
		View.ViewProjectionMatrix.GetInverse(),
		View.ViewOrigin,
		View.FarClip,
	};

	return Graph.AddPass("Selection", [RenderDevice, CommandList, PipelineState, SelectionDepth, Viewport = View.Viewport,
		ViewConstants, DrawCommands = std::move(DrawCommands)]()
	{
		ExecutePass(*RenderDevice, CommandList, PipelineState, SelectionDepth, Viewport, ViewConstants, DrawCommands);
	});
}

// 선택 Geometry의 전체 실루엣과 가장 가까운 표면 Depth를 전용 Target에 기록한다.
void FSelectionPass::ExecutePass(
	IRenderDevice& RenderDevice,
	FCommandListHandle CommandList,
	FPipelineStateHandle PipelineState,
	FTextureHandle SelectionDepth,
	const FRenderViewport& Viewport,
	const FViewConstants& ViewConstants,
	const TArray<FSelectionDrawCommand>& DrawCommands)
{
	RenderDevice.SetRenderTargets(CommandList, {}, SelectionDepth);
	RenderDevice.SetViewport(CommandList, Viewport);
	RenderDevice.SetPipelineState(CommandList, PipelineState);
	const auto* ViewBytes = reinterpret_cast<const uint8*>(&ViewConstants);
	RenderDevice.SetConstantData(CommandList, EShaderStage::Vertex, ViewConstantsSlot, std::span<const uint8>(ViewBytes, sizeof(ViewConstants)));

	for (const FSelectionDrawCommand& Command : DrawCommands)
	{
		const FDrawConstants DrawConstants = { Command.Primitive->WorldMatrix };
		const auto* DrawBytes = reinterpret_cast<const uint8*>(&DrawConstants);
		RenderDevice.SetConstantData(CommandList, EShaderStage::Vertex, DrawConstantsSlot, std::span<const uint8>(DrawBytes, sizeof(DrawConstants)));

		const FMeshBuffer& MeshBuffer = *Command.MeshBuffer;
		RenderDevice.SetVertexBuffer(CommandList, MeshBuffer.GetVertexBuffer().GetHandle(), MeshBuffer.GetStride());
		if (MeshBuffer.GetIndexCount() > 0)
		{
			RenderDevice.SetIndexBuffer(CommandList, MeshBuffer.GetIndexBuffer().GetHandle(), EIndexFormat::UInt32);
			RenderDevice.DrawIndexed(CommandList, Command.IndexCount, Command.FirstIndex);
		}
		else
		{
			RenderDevice.Draw(CommandList, Command.IndexCount, Command.FirstIndex);
		}
	}
}
