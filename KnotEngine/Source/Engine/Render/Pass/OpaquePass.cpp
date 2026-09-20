#include "Render/Pass/OpaquePass.h"

#include "Core/Assert.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Render/Renderer.h"
#include "Render/Resource/MaterialResource.h"
#include "Render/Resource/Mesh/MeshBuffer.h"
#include "Asset/Mesh/StaticMesh.h"
#include "Render/Resource/Mesh/StaticMeshResource.h"
#include "Render/Resource/TextureResource.h"
#include "Render/Resource/Mesh/Vertex.h"
#include "Render/RHI/RenderDevice.h"
#include "Render/Scene/SceneView.h"

#include <algorithm>
#include <cmath>

// Pipeline, Material, Mesh와 View Depth를 상태 변경 우선순위에 맞춰 64비트 정렬 키로 패킹한다.
uint64 FOpaquePass::GenerateSortKey(const FMaterialResource& Material, const FMeshBuffer& MeshBuffer, float Depth, float FarClip)
{
	static constexpr uint32 PipelineSortBitCount = 12;
	static constexpr uint32 MaterialSortBitCount = 20;
	static constexpr uint32 MeshSortBitCount = 20;
	static constexpr uint32 DepthSortBitCount = 12;
	static constexpr uint32 PipelineSortMask = (1u << PipelineSortBitCount) - 1;
	static constexpr uint32 MaterialSortMask = (1u << MaterialSortBitCount) - 1;
	static constexpr uint32 MeshSortMask = (1u << MeshSortBitCount) - 1;
	static constexpr uint32 DepthSortMask = (1u << DepthSortBitCount) - 1;

	check(!std::isnan(Depth) && !std::isinf(Depth));
	const uint32 PipelineSortId = Material.GetPipelineState().Index;
	const uint32 MaterialSortId = Material.GetSortId();
	const uint32 MeshSortId = MeshBuffer.GetVertexBuffer().GetHandle().Index;
	const float NormalizedDepth = std::clamp(Depth / std::max(FarClip, KMath::Epsilon), 0.0f, 1.0f);
	const uint32 DepthSortId = static_cast<uint32>(NormalizedDepth * DepthSortMask);
	checkf(PipelineSortId <= PipelineSortMask, "Opaque Pipeline Sort ID가 {}비트 범위를 초과했다.", PipelineSortBitCount);
	checkf(MaterialSortId <= MaterialSortMask, "Opaque Material Sort ID가 {}비트 범위를 초과했다.", MaterialSortBitCount);
	checkf(MeshSortId <= MeshSortMask, "Opaque Mesh Sort ID가 {}비트 범위를 초과했다.", MeshSortBitCount);

	return static_cast<uint64>(PipelineSortId & PipelineSortMask) << 52 |
	       static_cast<uint64>(MaterialSortId & MaterialSortMask) << 32 |
	       static_cast<uint64>(MeshSortId & MeshSortMask) << 12 | DepthSortId;
}

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
		check(StaticMeshProxy.MeshResource);
		const SIZE_T LODIndex = StaticMeshProxy.SelectLOD(View);
		const FStaticMeshLODResource& LOD = StaticMeshProxy.MeshResource->GetLOD(LODIndex);
		const FMeshBuffer* MeshBuffer = &LOD.GetMeshBuffer();
		checkf(MeshBuffer->IsValid(), "준비되지 않은 Static Mesh가 Opaque Pass에 전달되었다.");
		const float Depth = View.ViewMatrix.TransformPosition(Primitive->WorldBounds.GetCenter()).Z;

		for (const FStaticMeshSectionResource& Section : LOD.GetSections())
		{
			const FMaterialResource* Material = &StaticMeshProxy.GetMaterial(Section.MaterialIndex);

			FMeshDrawCommand Command;
			Command.Primitive = Primitive;
			Command.MeshBuffer = MeshBuffer;
			Command.Material = Material;
			Command.FirstIndex = Section.FirstIndex;
			Command.IndexCount = Section.IndexCount;
			Command.SortKey = GenerateSortKey(*Material, *MeshBuffer, Depth, View.FarClip);
			OpaqueCommands.push_back(std::move(Command));
		}
	}

	std::sort(OpaqueCommands.begin(), OpaqueCommands.end(), [](const FMeshDrawCommand& Left, const FMeshDrawCommand& Right)
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
	FPipelineStateHandle CurrentPipelineState;
	const FMaterialResource* CurrentMaterial = nullptr;
	const FMeshBuffer* CurrentMeshBuffer = nullptr;

	for (const FMeshDrawCommand& Command : OpaqueCommands)
	{
		check(Command.Material && Command.Material->IsValid());
		const FPipelineStateHandle PipelineState = Command.Material->GetPipelineState();
		if (CurrentPipelineState != PipelineState)
		{
			RenderDevice.SetPipelineState(CommandList, PipelineState);
			CurrentPipelineState = PipelineState;
		}
		if (CurrentMaterial != Command.Material)
		{
			if (!Command.Material->GetConstants().empty())
			{
				for (const FMaterialConstantBufferBinding& Binding : Command.Material->GetConstantBuffers())
				{
					RenderDevice.SetConstantData(CommandList, Binding.Stage, Binding.Slot, Command.Material->GetConstants());
				}
			}
			for (const FMaterialResource::FTextureBinding& Binding : Command.Material->GetTextures())
			{
				check(Binding.TextureResource && Binding.TextureResource->IsValid());
				RenderDevice.SetTexture(CommandList, Binding.Stage, Binding.TextureSlot, Binding.TextureResource->GetHandle());
				if (Binding.SamplerSlot != FSamplerHandle::InvalidIndex)
				{
					RenderDevice.SetSampler(CommandList, Binding.Stage, Binding.SamplerSlot, Binding.Sampler);
				}
			}
			CurrentMaterial = Command.Material;
		}

		const FMeshBuffer& MeshBuffer = *Command.MeshBuffer;
		checkf(MeshBuffer.IsValid(), "유효하지 않은 FMeshBuffer가 Opaque Pass에 전달되었다.");
		checkf(MeshBuffer.GetLayout() == FStaticMeshVertex::GetVertexLayout(), "Opaque Pipeline State와 호환되지 않는 Vertex Layout이다.");
		if (CurrentMeshBuffer != Command.MeshBuffer)
		{
			RenderDevice.SetVertexBuffer(CommandList, MeshBuffer.GetVertexBuffer().GetHandle(), MeshBuffer.GetStride());
			if (MeshBuffer.GetIndexCount() > 0)
			{
				RenderDevice.SetIndexBuffer(CommandList, MeshBuffer.GetIndexBuffer().GetHandle(), EIndexFormat::UInt32);
			}
			CurrentMeshBuffer = Command.MeshBuffer;
		}

		const FDrawConstants DrawConstants = { Command.Primitive->WorldMatrix };
		const auto* DrawBytes = reinterpret_cast<const uint8*>(&DrawConstants);
		RenderDevice.SetConstantData(CommandList, EShaderStage::Vertex, DrawConstantsSlot, std::span<const uint8>(DrawBytes, sizeof(DrawConstants)));
		if (MeshBuffer.GetIndexCount() > 0)
		{
			RenderDevice.DrawIndexed(CommandList, Command.IndexCount, Command.FirstIndex);
		}
		else
		{
			RenderDevice.Draw(CommandList, Command.IndexCount, Command.FirstIndex);
		}
	}
}
