#include "Render/Pass/OpaquePass.h"

#include "Core/Assert.h"
#include "Render/Proxy/MaterialRenderProxy.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Render/Renderer.h"
#include "Render/Resource/Mesh/MeshBuffer.h"
#include "Render/Resource/Mesh/Mesh.h"
#include "Render/Resource/Mesh/Vertex.h"
#include "Render/RHI/RenderDevice.h"
#include "Render/Scene/SceneView.h"

#include <algorithm>
#include <bit>
#include <cmath>

uint32 FOpaquePass::AddPass(FRenderGraph& Graph, URenderer& Renderer, const FSceneView& View, std::span<const FPrimitiveSceneProxy* const> VisiblePrimitives)
{
	IRenderDevice* RenderDevice = &Renderer.GetRenderDevice();

	const FCommandListHandle CommandList = Renderer.GetCommandList();
	check(CommandList.IsValid());

	TArray<FMeshDrawCommand> OpaqueCommands;
	OpaqueCommands.reserve(VisiblePrimitives.size());

	for (const FPrimitiveSceneProxy* Primitive : VisiblePrimitives)
	{
		const auto& StaticMeshProxy = static_cast<const FStaticMeshSceneProxy&>(*Primitive);
		check(StaticMeshProxy.Mesh);
		const FStaticMeshLOD& LOD = StaticMeshProxy.Mesh->GetLOD(StaticMeshProxy.SelectLOD(View));
		const FMeshBuffer* MeshBuffer = &LOD.GetMeshBuffer();
		checkf(MeshBuffer->IsValid(), "준비되지 않은 Static Mesh가 Opaque Pass에 전달되었다.");
		const float Depth = View.ViewMatrix.TransformPosition(Primitive->WorldBounds.GetCenter()).Z;
		check(!std::isnan(Depth) && !std::isinf(Depth));
		const uint32 SortKey = std::bit_cast<uint32>(std::max(0.0f, Depth));

		for (const FStaticMeshSection& Section : LOD.GetSections())
		{
			const UMaterialInterface* MaterialInterface = StaticMeshProxy.GetMaterial(Section.MaterialIndex);

			FMeshDrawCommand Command;
			Command.Primitive = Primitive;
			Command.MeshBuffer = MeshBuffer;
			Command.Material = &Renderer.RegisterMaterial(MaterialInterface);
			Command.FirstIndex = Section.FirstIndex;
			Command.IndexCount = Section.IndexCount;
			Command.SortKey = SortKey;
			OpaqueCommands.push_back(std::move(Command));
		}
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

	return Graph.AddPass("Opaque", [RenderDevice, CommandList, Viewport, ViewConstants, OpaqueCommands = std::move(OpaqueCommands)]()
	{
		ExecutePass(*RenderDevice, CommandList, Viewport, ViewConstants, OpaqueCommands);
	});
}

void FOpaquePass::ExecutePass(
    IRenderDevice& RenderDevice,
    FCommandListHandle CommandList,
    const FRenderViewport& Viewport,
    const FViewConstants& ViewConstants,
    const TArray<FMeshDrawCommand>& OpaqueCommands)
{
	RenderDevice.SetViewport(CommandList, Viewport);
	const auto* ViewBytes = reinterpret_cast<const uint8*>(&ViewConstants);
	RenderDevice.SetConstantData(CommandList, EShaderStage::Vertex, ViewConstantsSlot, std::span<const uint8>(ViewBytes, sizeof(ViewConstants)));

	for (const FMeshDrawCommand& Command : OpaqueCommands)
	{
		check(Command.Material && Command.Material->IsRegistered());
		RenderDevice.SetPipelineState(CommandList, Command.Material->GetPipelineState());
		if (!Command.Material->GetConstants().empty())
		{
			for (const FMaterialConstantBufferBinding& Binding : Command.Material->GetConstantBuffers())
			{
				RenderDevice.SetConstantData(CommandList, Binding.Stage, Binding.Slot, Command.Material->GetConstants());
			}
		}
		for (const FMaterialRenderProxy::FTextureBinding& Binding : Command.Material->GetTextures())
		{
			RenderDevice.SetTexture(CommandList, Binding.Stage, Binding.TextureSlot, Binding.Texture);
			if (Binding.SamplerSlot != FSamplerHandle::InvalidIndex)
			{
				RenderDevice.SetSampler(CommandList, Binding.Stage, Binding.SamplerSlot, Binding.Sampler);
			}
		}

		const FMeshBuffer& MeshBuffer = *Command.MeshBuffer;
		checkf(MeshBuffer.IsValid(), "유효하지 않은 FMeshBuffer가 Opaque Pass에 전달되었다.");
		checkf(MeshBuffer.GetLayout() == FStaticMeshVertex::GetVertexLayout(), "Opaque Pipeline State와 호환되지 않는 Vertex Layout이다.");

		const FDrawConstants DrawConstants = { Command.Primitive->WorldMatrix };
		const auto* DrawBytes = reinterpret_cast<const uint8*>(&DrawConstants);
		RenderDevice.SetConstantData(CommandList, EShaderStage::Vertex, DrawConstantsSlot, std::span<const uint8>(DrawBytes, sizeof(DrawConstants)));
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
