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
#include <chrono>
#include <limits>

bool FOpaquePass::CanBatch(const FPrimitiveCommand& Left, const FPrimitiveCommand& Right)
{
	if (Left.LOD != Right.LOD)
	{
		return false;
	}
	for (const FStaticMeshSectionResource& Section : Left.LOD->GetSections())
	{
		if (&Left.Primitive->GetMaterial(Section.MaterialIndex) != &Right.Primitive->GetMaterial(Section.MaterialIndex))
		{
			return false;
		}
	}
	return true;
}

bool FOpaquePass::CanInstance(const FPrimitiveCommand& Command)
{
	if (Command.MeshBuffer->GetIndexCount() == 0)
	{
		return false;
	}
	for (const FStaticMeshSectionResource& Section : Command.LOD->GetSections())
	{
		if (!Command.Primitive->GetMaterial(Section.MaterialIndex).GetInstancedPipelineState().IsValid())
		{
			return false;
		}
	}
	return true;
}

// 현재 View Mode에 필요한 Surface 또는 Wireframe Pipeline을 Cache에서 지연 생성한다.
FPipelineStateHandle FOpaquePass::GetOrCreatePipelineState(
	FRenderer& Renderer,
	const FMaterialResource& Material,
	bool bInstanced,
	EViewMode ViewMode,
	bool bOverlay)
{
	const FPipelineStateHandle BasePipeline = bInstanced ? Material.GetInstancedPipelineState() : Material.GetPipelineState();
	if (ViewMode != EViewMode::Wireframe && !bOverlay)
	{
		return BasePipeline;
	}

	return Renderer.GetPipelineStateCache().GetOrCreateWireframe(BasePipeline, bOverlay, [&]()
	{
		// 변형이 없을 때만 Description 복사와 Shader 조회를 수행한다.
		FPipelineStateDesc Desc = Material.GetPipelineStateDesc(bInstanced);
		Desc.PixelShader = Renderer.GetShaderRegistry().GetOrCreate({
			"/Engine/Shader/StaticMesh.hlsl",
			bOverlay ? "WireOverlayPS" : "WireframePS",
			EShaderStage::Pixel,
		});
		Desc.RasterizerState.FillMode = EFillMode::Wireframe;
		Desc.RasterizerState.CullMode = ECullMode::None;
		Desc.BlendState = {};
		if (bOverlay)
		{
			Desc.DepthMode = EDepthMode::ReadOnly;
			FRenderTargetBlendDesc& Blend = Desc.BlendState.RenderTarget;
			Blend.bBlendEnabled = true;
			Blend.SourceColorBlend = EBlendFactor::SourceAlpha;
			Blend.DestinationColorBlend = EBlendFactor::InverseSourceAlpha;
			Blend.SourceAlphaBlend = EBlendFactor::One;
			Blend.DestinationAlphaBlend = EBlendFactor::InverseSourceAlpha;
			// Reversed-Z에서 양의 Bias로 표면 위 격자의 Z Fighting을 줄인다.
			Desc.RasterizerState.DepthBias = 1;
			Desc.RasterizerState.SlopeScaledDepthBias = 1.0f;
		}
		else
		{
			Desc.DepthMode = EDepthMode::ReadWrite;
		}
		return Desc;
	});
}

// Pipeline, Material과 Mesh를 상태 변경 우선순위에 맞춰 64비트 정렬 키로 패킹한다.
uint64 FOpaquePass::GenerateSortKey(FPipelineStateHandle PipelineState, const FMaterialResource& Material, const FMeshBuffer& MeshBuffer)
{
	static constexpr uint32 PipelineSortBitCount = 12;
	static constexpr uint32 MaterialSortBitCount = 20;
	static constexpr uint32 MeshSortBitCount = 20;
	static constexpr uint32 PipelineSortMask = (1u << PipelineSortBitCount) - 1;
	static constexpr uint32 MaterialSortMask = (1u << MaterialSortBitCount) - 1;
	static constexpr uint32 MeshSortMask = (1u << MeshSortBitCount) - 1;

	const uint32 PipelineSortId = PipelineState.Index;
	const uint32 MaterialSortId = Material.GetSortId();
	const uint32 MeshSortId = MeshBuffer.GetVertexBuffer().GetHandle().Index;
	checkf(PipelineSortId <= PipelineSortMask, "Opaque Pipeline Sort ID가 {}비트 범위를 초과했다.", PipelineSortBitCount);
	checkf(MaterialSortId <= MaterialSortMask, "Opaque Material Sort ID가 {}비트 범위를 초과했다.", MaterialSortBitCount);
	checkf(MeshSortId <= MeshSortMask, "Opaque Mesh Sort ID가 {}비트 범위를 초과했다.", MeshSortBitCount);

	return static_cast<uint64>(PipelineSortId & PipelineSortMask) << 52 |
	       static_cast<uint64>(MaterialSortId & MaterialSortMask) << 32 |
	       static_cast<uint64>(MeshSortId & MeshSortMask) << 12;
}

uint32 FOpaquePass::AddPass(FRenderGraph& Graph, FRenderer& Renderer, const FSceneView& View, std::span<const FPrimitiveSceneProxy* const> VisiblePrimitives)
{
	const auto BatchBuildStart = std::chrono::steady_clock::now();
	FInstancedDrawStatistics Statistics;
	Statistics.VisiblePrimitives = VisiblePrimitives.size();
	const FCommandListHandle CommandList = Renderer.GetCommandList();
	check(CommandList.IsValid());

	TArray<FPrimitiveCommand> PrimitiveCommands;
	PrimitiveCommands.reserve(VisiblePrimitives.size());
	for (const FPrimitiveSceneProxy* Primitive : VisiblePrimitives)
	{
		const auto& StaticMeshProxy = static_cast<const FStaticMeshSceneProxy&>(*Primitive);
		check(StaticMeshProxy.MeshResource);
		const FStaticMeshLODResource& LOD = StaticMeshProxy.MeshResource->GetLOD(StaticMeshProxy.SelectLOD(View));
		const FMeshBuffer& MeshBuffer = LOD.GetMeshBuffer();
		checkf(MeshBuffer.IsValid(), "준비되지 않은 Static Mesh가 Opaque Pass에 전달되었다.");

		uint64 BatchKey = reinterpret_cast<uintptr_t>(&LOD);
		for (const FStaticMeshSectionResource& Section : LOD.GetSections())
		{
			const uint64 MaterialKey = reinterpret_cast<uintptr_t>(&StaticMeshProxy.GetMaterial(Section.MaterialIndex));
			BatchKey ^= MaterialKey + 0x9e3779b97f4a7c15ull + (BatchKey << 6) + (BatchKey >> 2);
		}
		PrimitiveCommands.push_back({ &StaticMeshProxy, &LOD, &MeshBuffer, BatchKey });
	}
	std::sort(PrimitiveCommands.begin(), PrimitiveCommands.end(), [](const FPrimitiveCommand& Left, const FPrimitiveCommand& Right)
	{
		return Left.BatchKey < Right.BatchKey;
	});

	TArray<FMeshDrawCommand> OpaqueCommands;
	TArray<FStaticMeshInstance> Instances;
	Instances.reserve(PrimitiveCommands.size());
	for (SIZE_T FirstPrimitive = 0; FirstPrimitive < PrimitiveCommands.size();)
	{
		const FPrimitiveCommand& FirstCommand = PrimitiveCommands[FirstPrimitive];
		SIZE_T LastPrimitive = FirstPrimitive + 1;
		while (LastPrimitive < PrimitiveCommands.size() &&
			FirstCommand.BatchKey == PrimitiveCommands[LastPrimitive].BatchKey &&
			CanBatch(FirstCommand, PrimitiveCommands[LastPrimitive]))
		{
			++LastPrimitive;
		}

		const SIZE_T PrimitiveCount = LastPrimitive - FirstPrimitive;
		const bool bInstanced = PrimitiveCount >= MinimumInstanceCount && CanInstance(FirstCommand);
		uint32 FirstInstance = 0;
		if (bInstanced)
		{
			Statistics.InstancedPrimitives += PrimitiveCount;
			++Statistics.InstanceBatches;
			checkf(Instances.size() <= (std::numeric_limits<uint32>::max)() - PrimitiveCount,
				"Static Mesh Instance 수가 uint32 범위를 초과했다.");
			FirstInstance = static_cast<uint32>(Instances.size());
			for (SIZE_T PrimitiveIndex = FirstPrimitive; PrimitiveIndex < LastPrimitive; ++PrimitiveIndex)
			{
				Instances.push_back({ PrimitiveCommands[PrimitiveIndex].Primitive->WorldMatrix });
			}
		}

		const FStaticMeshLODResource& LOD = *FirstCommand.LOD;
		for (const FStaticMeshSectionResource& Section : LOD.GetSections())
		{
			if (bInstanced)
			{
				++Statistics.InstancedDrawCalls;
				const FMaterialResource* Material = &FirstCommand.Primitive->GetMaterial(Section.MaterialIndex);
				const FPipelineStateHandle PipelineState = GetOrCreatePipelineState(Renderer, *Material, true, View.ViewMode, false);
				const FPipelineStateHandle OverlayPipelineState = View.ViewMode == EViewMode::ShadedWireframe
					? GetOrCreatePipelineState(Renderer, *Material, true, View.ViewMode, true)
					: FPipelineStateHandle{};
				OpaqueCommands.push_back({
					nullptr,
					FirstCommand.MeshBuffer,
					Material,
					Section.FirstIndex,
					Section.IndexCount,
					FirstInstance,
					static_cast<uint32>(PrimitiveCount),
					GenerateSortKey(PipelineState, *Material, *FirstCommand.MeshBuffer),
					PipelineState,
					OverlayPipelineState,
					true,
				});
				continue;
			}

			Statistics.FallbackDrawCalls += PrimitiveCount;
			for (SIZE_T PrimitiveIndex = FirstPrimitive; PrimitiveIndex < LastPrimitive; ++PrimitiveIndex)
			{
				const FPrimitiveCommand& PrimitiveCommand = PrimitiveCommands[PrimitiveIndex];
				const FMaterialResource* Material = &PrimitiveCommand.Primitive->GetMaterial(Section.MaterialIndex);
				const FPipelineStateHandle PipelineState = GetOrCreatePipelineState(Renderer, *Material, false, View.ViewMode, false);
				const FPipelineStateHandle OverlayPipelineState = View.ViewMode == EViewMode::ShadedWireframe
					? GetOrCreatePipelineState(Renderer, *Material, false, View.ViewMode, true)
					: FPipelineStateHandle{};
				OpaqueCommands.push_back({
					PrimitiveCommand.Primitive,
					PrimitiveCommand.MeshBuffer,
					Material,
					Section.FirstIndex,
					Section.IndexCount,
					0,
					0,
					GenerateSortKey(PipelineState, *Material, *PrimitiveCommand.MeshBuffer),
					PipelineState,
					OverlayPipelineState,
					false,
				});
			}
		}
		FirstPrimitive = LastPrimitive;
	}

	std::sort(OpaqueCommands.begin(), OpaqueCommands.end(), [](const FMeshDrawCommand& Left, const FMeshDrawCommand& Right)
	{
		return Left.SortKey < Right.SortKey;
	});
	Statistics.BatchBuildTimeMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - BatchBuildStart).count();
	
	if (View.ViewMode == EViewMode::ShadedWireframe)
	{
		Statistics.InstancedDrawCalls *= 2;
		Statistics.FallbackDrawCalls *= 2;
	}

	Renderer.AccumulateInstancedDrawStatistics(Statistics);

	const FViewConstants ViewConstants = {
		View.ViewProjectionMatrix,
		View.ViewProjectionMatrix.GetInverse(),
		View.ViewOrigin,
		View.FarClip,
	};

	const FRenderViewport Viewport = View.Viewport;

	return Graph.AddPass("Opaque", [&Renderer, CommandList, Viewport, ViewConstants, ViewMode = View.ViewMode,
		OpaqueCommands = std::move(OpaqueCommands), Instances = std::move(Instances)]()
	{
		ExecutePass(Renderer, CommandList, Viewport, ViewConstants, ViewMode, OpaqueCommands, Instances);
	});
}

void FOpaquePass::ExecutePass(
	FRenderer& Renderer,
	FCommandListHandle CommandList,
	const FRenderViewport& Viewport,
	const FViewConstants& ViewConstants,
	EViewMode ViewMode,
	const TArray<FMeshDrawCommand>& OpaqueCommands,
	const TArray<FStaticMeshInstance>& Instances)
{
	IRenderDevice& RenderDevice = Renderer.GetRenderDevice();
	RenderDevice.SetViewport(CommandList, Viewport);
	const auto* ViewBytes = reinterpret_cast<const uint8*>(&ViewConstants);
	RenderDevice.SetConstantData(CommandList, EShaderStage::Vertex, ViewConstantsSlot, std::span<const uint8>(ViewBytes, sizeof(ViewConstants)));
	FBufferHandle InstanceBuffer;
	if (!Instances.empty())
	{
		const auto UploadStart = std::chrono::steady_clock::now();
		InstanceBuffer = Renderer.UploadStaticMeshInstances(Instances);
		FInstancedDrawStatistics Statistics;
		Statistics.InstanceUploadTimeMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - UploadStart).count();
		Renderer.AccumulateInstancedDrawStatistics(Statistics);
	}
	// 전체 표면을 먼저 그린 뒤 동일한 Instance Buffer로 가려지지 않은 격자만 덧그린다.
	const uint32 DrawPassCount = ViewMode == EViewMode::ShadedWireframe ? 2 : 1;
	for (uint32 DrawPassIndex = 0; DrawPassIndex < DrawPassCount; ++DrawPassIndex)
	{
		const bool bOverlay = DrawPassIndex == 1;
		FPipelineStateHandle CurrentPipelineState;
		const FMaterialResource* CurrentMaterial = nullptr;
		const FMeshBuffer* CurrentMeshBuffer = nullptr;
		bool bCurrentMeshInstanced = false;

		for (const FMeshDrawCommand& Command : OpaqueCommands)
		{
			verify(Command.Material && Command.Material->IsValid());
			const FPipelineStateHandle PipelineState = bOverlay ? Command.OverlayPipelineState : Command.PipelineState;
			check(PipelineState.IsValid());
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
			if (CurrentMeshBuffer != Command.MeshBuffer || bCurrentMeshInstanced != Command.bInstanced)
			{
				if (Command.bInstanced)
				{
					check(InstanceBuffer.IsValid());
					const FVertexBufferBinding Bindings[] = {
						{ MeshBuffer.GetVertexBuffer().GetHandle(), MeshBuffer.GetStride(), 0 },
						{ InstanceBuffer, static_cast<uint32>(sizeof(FStaticMeshInstance)), 0 },
					};
					RenderDevice.SetVertexBuffers(CommandList, 0, Bindings);
				}
				else
				{
					RenderDevice.SetVertexBuffer(CommandList, MeshBuffer.GetVertexBuffer().GetHandle(), MeshBuffer.GetStride());
				}
				if (CurrentMeshBuffer != Command.MeshBuffer && MeshBuffer.GetIndexCount() > 0)
				{
					RenderDevice.SetIndexBuffer(CommandList, MeshBuffer.GetIndexBuffer().GetHandle(), EIndexFormat::UInt32);
				}
				CurrentMeshBuffer = Command.MeshBuffer;
				bCurrentMeshInstanced = Command.bInstanced;
			}

			if (Command.bInstanced)
			{
				RenderDevice.DrawIndexedInstanced(
					CommandList,
					Command.IndexCount,
					Command.InstanceCount,
					Command.FirstIndex,
					0,
					Command.FirstInstance);
			}
			else
			{
				check(Command.Primitive);
				const auto* DrawBytes = reinterpret_cast<const uint8*>(&Command.Primitive->WorldMatrix);
				RenderDevice.SetConstantData(
					CommandList,
					EShaderStage::Vertex,
					DrawConstantsSlot,
					std::span<const uint8>(DrawBytes, sizeof(FMatrix)));
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
	}
}
