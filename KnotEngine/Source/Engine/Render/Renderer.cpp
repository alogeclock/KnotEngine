#include "Render/Renderer.h"

#include "Core/Assert.h"
#include "Core/Profiling/CPUProfiler.h"
#include "Render/Graph/RenderGraph.h"
#include "Render/RHI/RenderContext.h"
#include "Render/RHI/RenderDevice.h"
#include "Render/Resource/ResourceCommand.h"
#include "Render/Resource/Mesh/Vertex.h"

#include <algorithm>
#include <limits>

FRenderer::FRenderer(IRenderDevice& InRenderDevice, IRenderContext& InRenderContext, IShaderFormat& InShaderFormat)
	: RenderDevice(InRenderDevice),
	  RenderContext(InRenderContext),
	  ShaderCompiler(InShaderFormat),
	  ShaderRegistry(InRenderDevice, ShaderCompiler),
	  PipelineStateCache(InRenderDevice),
	  SamplerStateCache(InRenderDevice)
{
}

FRenderer::~FRenderer()
{
	Release();
}

IRenderDevice& FRenderer::GetRenderDevice() const
{
	return RenderDevice;
}

FShaderRegistry& FRenderer::GetShaderRegistry()
{
	return ShaderRegistry;
}

FPipelineStateCache& FRenderer::GetPipelineStateCache()
{
	return PipelineStateCache;
}

FSamplerStateCache& FRenderer::GetSamplerStateCache()
{
	return SamplerStateCache;
}

FCommandListHandle FRenderer::GetCommandList() const
{
	return CommandList;
}

FRenderViewport FRenderer::GetViewport() const
{
	return RenderContext.GetViewport();
}

void FRenderer::Create(void* NativeWindowHandle)
{
	Release();
	RenderDevice.Create();
	RenderContext.Create(NativeWindowHandle);
	ShaderRegistry.Create();
	PipelineStateCache.Create();
	SamplerStateCache.Create();

	static constexpr uint8 WhitePixel[] = { 255, 255, 255, 255 };
	FTextureDesc WhiteTextureDesc;
	WhiteTextureDesc.Width = 1;
	WhiteTextureDesc.Height = 1;
	WhiteTextureDesc.bSRGB = true;
	const FTextureSubresourceData WhiteTextureData = { WhitePixel, sizeof(WhitePixel), sizeof(WhitePixel) };
	panicf(DefaultTextureResource.Initialize(RenderDevice, WhiteTextureDesc, std::span(&WhiteTextureData, 1), 1), "기본 White Texture 생성에 실패했다.");
	FMaterialResourceCommand DefaultMaterialCommand;
	DefaultMaterialCommand.Revision = 1;
	panicf(DefaultMaterialResource.Initialize(*this, DefaultMaterialCommand, 0), "기본 Material Resource 생성에 실패했다.");
	DebugDraw.Create();
}

void FRenderer::Release()
{
	checkf(!CommandList.IsValid(), "열린 Render Command List가 있는 상태에서 Renderer를 해제할 수 없다.");
	ReleaseAssetReferences();
	DefaultMaterialResource.Release();
	DebugDraw.Release();
	RenderDevice.DestroyBuffer(StaticMeshInstanceBuffer);
	StaticMeshInstanceBufferCapacity = 0;
	DefaultTextureResource.Release();
	SamplerStateCache.Release();
	PipelineStateCache.Release();
	ShaderRegistry.Release();
	RenderContext.Release();
	RenderDevice.Release();
}

// 현재 Pass가 사용하는 Static Mesh Instance를 동적 Vertex Buffer에 한 번 업로드한다.
FBufferHandle FRenderer::UploadStaticMeshInstances(std::span<const FStaticMeshInstance> Instances)
{
	check(!Instances.empty());
	checkf(Instances.size() <= (std::numeric_limits<uint32>::max)() / sizeof(FStaticMeshInstance),
		"Static Mesh Instance Buffer 크기가 uint32 범위를 초과했다. Count={}", Instances.size());
	const uint32 RequiredCapacity = static_cast<uint32>(Instances.size());
	if (RequiredCapacity > StaticMeshInstanceBufferCapacity)
	{
		uint32 NewCapacity = std::max(4096u, StaticMeshInstanceBufferCapacity);
		while (NewCapacity < RequiredCapacity)
		{
			check(NewCapacity <= (std::numeric_limits<uint32>::max)() / 2);
			NewCapacity *= 2;
		}
		check(NewCapacity <= (std::numeric_limits<uint32>::max)() / sizeof(FStaticMeshInstance));
		RenderDevice.DestroyBuffer(StaticMeshInstanceBuffer);
		const FBufferDesc Desc = {
			NewCapacity * static_cast<uint32>(sizeof(FStaticMeshInstance)), EBufferUsage::Vertex, EResourceAccess::CPUWrite
		};
		StaticMeshInstanceBuffer = RenderDevice.CreateBuffer(Desc);
		StaticMeshInstanceBufferCapacity = NewCapacity;
	}

	const auto* Bytes = reinterpret_cast<const uint8*>(Instances.data());
	RenderDevice.UpdateBuffer(StaticMeshInstanceBuffer, std::span<const uint8>(Bytes, Instances.size_bytes()));
	return StaticMeshInstanceBuffer;
}

void FRenderer::AccumulateInstancedDrawStatistics(const FInstancedDrawStatistics& Statistics)
{
	InstancedDrawStatistics.VisiblePrimitives += Statistics.VisiblePrimitives;
	InstancedDrawStatistics.InstancedPrimitives += Statistics.InstancedPrimitives;
	InstancedDrawStatistics.InstanceBatches += Statistics.InstanceBatches;
	InstancedDrawStatistics.InstancedDrawCalls += Statistics.InstancedDrawCalls;
	InstancedDrawStatistics.FallbackDrawCalls += Statistics.FallbackDrawCalls;
	InstancedDrawStatistics.BatchBuildTimeMs += Statistics.BatchBuildTimeMs;
	InstancedDrawStatistics.InstanceUploadTimeMs += Statistics.InstanceUploadTimeMs;
}

// Asset별 GPU Resource를 해제하고 Renderer가 보관한 참조를 비운다.
void FRenderer::ReleaseAssetReferences()
{
	checkf(!CommandList.IsValid(), "열린 Render Command List가 있는 상태에서 Asset 참조를 해제할 수 없다.");
	MaterialResources.clear();
	TextureResources.clear();
	StaticMeshResources.clear();
}

void FRenderer::UpdateTextureResource(const FTextureResourceCommand& Command)
{
	check(Command.AssetId.IsValid() && Command.Revision != 0);
	std::unique_ptr<FTextureResource>& Resource = TextureResources[Command.AssetId];
	if (!Resource)
	{
		Resource = std::make_unique<FTextureResource>();
	}
	if (Resource->GetSourceRevision() == Command.Revision)
	{
		return;
	}

	TArray<FTextureSubresourceData> Subresources;
	Subresources.reserve(Command.Mips.size());
	for (const FTextureMipData& Mip : Command.Mips)
	{
		Subresources.push_back({ Mip.Bytes, Mip.RowPitch, static_cast<uint32>(Mip.Bytes.size()) });
	}
	panicf(Resource->Initialize(RenderDevice, Command.Desc, Subresources, Command.Revision),
	       "Texture Resource 생성에 실패했다. AssetId={}", Command.AssetId.ToString());
}

void FRenderer::UpdateStaticMeshResource(const FStaticMeshResourceCommand& Command)
{
	check(Command.AssetId.IsValid() && Command.Revision != 0);
	std::unique_ptr<FStaticMeshResource>& Resource = StaticMeshResources[Command.AssetId];
	if (!Resource)
	{
		Resource = std::make_unique<FStaticMeshResource>();
	}
	if (Resource->GetSourceRevision() != Command.Revision)
	{
		panicf(Resource->Initialize(RenderDevice, Command.Mesh, Command.Revision),
		       "Static Mesh Resource 생성에 실패했다. AssetId={}", Command.AssetId.ToString());
	}
}

void FRenderer::UpdateMaterialResource(const FMaterialResourceCommand& Command)
{
	check(Command.AssetId.IsValid() && Command.Revision != 0);
	std::unique_ptr<FMaterialResource>& Resource = MaterialResources[Command.AssetId];
	if (!Resource)
	{
		Resource = std::make_unique<FMaterialResource>();
	}
	if (Resource->GetSourceRevision() != Command.Revision)
	{
		panicf(MaterialResources.size() < (std::numeric_limits<uint32>::max)(), "Material Resource Sort ID가 uint32 범위를 초과했다.");
		const uint32 SortId = Resource->IsValid() ? Resource->GetSortId() : static_cast<uint32>(MaterialResources.size());
		panicf(Resource->Initialize(*this, Command, SortId), "Material Resource 생성에 실패했다. AssetId={}", Command.AssetId.ToString());
	}
}

// 지정한 소스 파일에 등록된 Shader Key를 반환하며 빈 경로이면 전체 Key를 반환한다.
TArray<FShaderKey> FRenderer::GetShaderKeysForSource(const FString& SourcePath) const
{
	return ShaderRegistry.GetKeysForSource(SourcePath);
}

// 컴파일된 Shader로 GPU Shader와 영향받는 PSO를 준비하고, 필요하면 Material 바인딩까지 Frame 경계에서 교체한다.
bool FRenderer::ReloadShaders(const TArray<FShaderReloadEntry>& Compiled, const TArray<FMaterialResourceCommand>& MaterialCommands, FString& Diagnostics)
{
	checkf(!CommandList.IsValid(), "열린 Render Command List에서 Shader를 교체할 수 없다.");
	if (!ShaderRegistry.StageReload(Compiled, Diagnostics))
	{
		return false;
	}

	const TArray<std::pair<FShaderHandle, FShaderHandle>> Replacements = ShaderRegistry.GetStagedReplacements();
	PipelineStateCache.BeginReload();
	if (!PipelineStateCache.StageAffectedPipelines(Replacements, Diagnostics))
	{
		PipelineStateCache.CancelReload();
		ShaderRegistry.CancelReload();
		return false;
	}

	// 기존 Material의 일반/Instanced Pipeline이 교체 대상 Shader를 사용하는지 확인한다.
	const auto UsesReplacedShader = [&Replacements](const FMaterialResource& Material)
	{
		for (bool bInstanced : { false, true })
		{
			const FPipelineStateDesc& Desc = Material.GetPipelineStateDesc(bInstanced);
			for (const auto& Replacement : Replacements)
			{
				if (Desc.VertexShader == Replacement.first || Desc.PixelShader == Replacement.first)
				{
					return true;
				}
			}
		}
		return false;
	};
	const auto NeedsBindingRebuild = [this](const FMaterialResource& Material)
	{
		for (bool bInstanced : { false, true })
		{
			const FPipelineStateDesc& Desc = Material.GetPipelineStateDesc(bInstanced);
			if (ShaderRegistry.HasStagedMaterialLayoutChange(Desc.VertexShader) || ShaderRegistry.HasStagedMaterialLayoutChange(Desc.PixelShader))
			{
				return true;
			}
		}
		return false;
	};
	struct FPipelineUpdate
	{
		FMaterialResource* Material = nullptr;
		bool bInstanced = false;
		FPipelineStateDesc Desc;
		FPipelineStateHandle Handle;
	};
	TArray<FPipelineUpdate> PipelineUpdates;
	const auto StagePipelineUpdates = [this, &Replacements, &PipelineUpdates, &Diagnostics](FMaterialResource& Material)
	{
		for (bool bInstanced : { false, true })
		{
			FPipelineStateDesc Desc = Material.GetPipelineStateDesc(bInstanced);
			bool bChanged = false;
			for (const auto& [OldShader, NewShader] : Replacements)
			{
				if (Desc.VertexShader == OldShader)
				{
					Desc.VertexShader = NewShader;
					bChanged = true;
				}
				if (Desc.PixelShader == OldShader)
				{
					Desc.PixelShader = NewShader;
					bChanged = true;
				}
			}
			if (!bChanged)
			{
				continue;
			}
			FPipelineStateHandle Handle;
			if (!PipelineStateCache.TryGetOrCreate(Desc, Handle, Diagnostics))
			{
				return false;
			}
			PipelineUpdates.push_back({ &Material, bInstanced, std::move(Desc), Handle });
		}
		return true;
	};

	FMaterialResource NewDefault;
	const bool bDefaultAffected = UsesReplacedShader(DefaultMaterialResource);
	const bool bRebuildDefaultBindings = bDefaultAffected && NeedsBindingRebuild(DefaultMaterialResource);
	if (bRebuildDefaultBindings)
	{
		FMaterialResourceCommand DefaultCommand;
		DefaultCommand.Revision = DefaultMaterialResource.GetSourceRevision();
		if (!NewDefault.Initialize(*this, DefaultCommand, 0, &Diagnostics))
		{
			PipelineStateCache.CancelReload();
			ShaderRegistry.CancelReload();
			return false;
		}
	}
	else if (bDefaultAffected && !StagePipelineUpdates(DefaultMaterialResource))
	{
		PipelineStateCache.CancelReload();
		ShaderRegistry.CancelReload();
		return false;
	}

	TMap<FAssetId, std::unique_ptr<FMaterialResource>, FAssetIdHash> PreparedMaterials;
	for (const FMaterialResourceCommand& Command : MaterialCommands)
	{
		const auto Existing = MaterialResources.find(Command.AssetId);
		check(Existing != MaterialResources.end() && Existing->second);
		check(Command.Revision == Existing->second->GetSourceRevision());
		if (!NeedsBindingRebuild(*Existing->second))
		{
			if (!StagePipelineUpdates(*Existing->second))
			{
				PipelineStateCache.CancelReload();
				ShaderRegistry.CancelReload();
				return false;
			}
			continue;
		}
		auto Candidate = std::make_unique<FMaterialResource>();
		if (!Candidate->Initialize(*this, Command, Existing->second->GetSortId(), &Diagnostics))
		{
			PipelineStateCache.CancelReload();
			ShaderRegistry.CancelReload();
			return false;
		}
		PreparedMaterials.emplace(Command.AssetId, std::move(Candidate));
	}

	// D3D11 EVENT Query로 이전 프레임의 실제 GPU 완료를 확인한다. RT CPU Flush만으로는 자원 폐기가 안전하지 않다.
	if (!RenderDevice.WaitForIdle())
	{
		Diagnostics = "이전 GPU 작업 완료를 확인하지 못해 Shader 교체를 취소했다.";
		PipelineStateCache.CancelReload();
		ShaderRegistry.CancelReload();
		return false;
	}

	ShaderRegistry.CommitReload();
	const TArray<FShaderHandle> OldHandles = ShaderRegistry.GetReplacedHandles();

	if (bRebuildDefaultBindings)
	{
		DefaultMaterialResource.Swap(NewDefault);
	}
	for (auto& [AssetId, Candidate] : PreparedMaterials)
	{
		MaterialResources.at(AssetId)->Swap(*Candidate);
	}
	for (FPipelineUpdate& Update : PipelineUpdates)
	{
		Update.Material->SetPipelineState(Update.bInstanced, std::move(Update.Desc), Update.Handle);
	}

	PipelineStateCache.CommitReload(OldHandles);
	ShaderRegistry.ReleaseReplacedShaders();
	Diagnostics.clear();
	return true;
}

FTextureResource* FRenderer::FindTextureResource(const FAssetId& AssetId) const
{
	const auto Iterator = TextureResources.find(AssetId);
	return Iterator != TextureResources.end() ? Iterator->second.get() : nullptr;
}

FStaticMeshResource* FRenderer::FindStaticMeshResource(const FAssetId& AssetId) const
{
	const auto Iterator = StaticMeshResources.find(AssetId);
	return Iterator != StaticMeshResources.end() ? Iterator->second.get() : nullptr;
}

FMaterialResource* FRenderer::FindMaterialResource(const FAssetId& AssetId) const
{
	const auto Iterator = MaterialResources.find(AssetId);
	return Iterator != MaterialResources.end() ? Iterator->second.get() : nullptr;
}

void FRenderer::Resize(uint32 Width, uint32 Height)
{
	checkf(!CommandList.IsValid(), "열린 Render Command List가 있는 상태에서 Render Context 크기를 변경할 수 없다.");
	RenderContext.Resize(Width, Height);
}

void FRenderer::BeginFrame()
{
	KNOT_PROFILE_SCOPE("Render", "FRenderer::BeginFrame");

	checkf(!CommandList.IsValid(), "Renderer Frame이 이미 시작되었다.");
	InstancedDrawStatistics = {};
	CommandList = RenderDevice.BeginCommandList();
	RenderDevice.BeginFrameStatistics(CommandList);
	RenderContext.BeginFrame(CommandList);
}

void FRenderer::EndFrame()
{
	KNOT_PROFILE_SCOPE("Render", "FRenderer::EndFrame");

	checkf(CommandList.IsValid(), "Renderer Frame이 시작되지 않았다.");
	RenderContext.EndFrame(CommandList);
	RenderDevice.EndFrameStatistics(CommandList);
	RenderDevice.EndCommandList(CommandList);
	RenderDevice.Submit(CommandList);
	RenderContext.Present();
	DebugDraw.Reset();
}

void FRenderer::Execute(FRenderGraph& RenderGraph)
{
	check(CommandList.IsValid());
	RenderGraph.Execute();
}

// World를 offscreen Color/Depth Target에 렌더링하도록 출력 대상과 Viewport를 설정하고 이전 프레임의 내용을 초기화한다.
void FRenderer::BeginRenderTarget(FTextureHandle ColorTarget, FTextureHandle DepthTarget, const FRenderViewport& Viewport)
{
	check(CommandList.IsValid());
	static constexpr float ViewportClearColor[4] = { 0.035f, 0.04f, 0.05f, 1.0f };
	RenderDevice.SetRenderTargets(CommandList, ColorTarget, DepthTarget);
	RenderDevice.SetViewport(CommandList, Viewport);
	RenderDevice.ClearRenderTarget(CommandList, ColorTarget, ViewportClearColor);
	RenderDevice.ClearDepthStencil(CommandList, DepthTarget, 0.0f, 0);
}

// Offscreen RTV 바인딩을 끝내고 Back Buffer를 복구하여 이후 ImGui가 Color Target의 SRV를 화면에 렌더링할 수 있게 한다.
void FRenderer::EndRenderTarget()
{
	check(CommandList.IsValid());
	RenderContext.BindBackBuffer(CommandList);
}
