#include "Render/Renderer.h"

#include "Core/Assert.h"
#include "Core/Profiling/CPUProfiler.h"
#include "Render/Graph/RenderGraph.h"
#include "Render/RHI/RenderContext.h"
#include "Render/RHI/RenderDevice.h"
#include "Render/Resource/ResourceCommand.h"

#include <limits>

URenderer::URenderer(IRenderDevice& InRenderDevice, IRenderContext& InRenderContext, IShaderFormat& InShaderFormat)
	: RenderDevice(InRenderDevice),
	  RenderContext(InRenderContext),
	  ShaderCompiler(InShaderFormat),
	  ShaderRegistry(InRenderDevice, ShaderCompiler),
	  PipelineStateCache(InRenderDevice),
	  SamplerStateCache(InRenderDevice)
{
}

URenderer::~URenderer()
{
	Release();
}

IRenderDevice& URenderer::GetRenderDevice() const
{
	return RenderDevice;
}

FShaderRegistry& URenderer::GetShaderRegistry()
{
	return ShaderRegistry;
}

FPipelineStateCache& URenderer::GetPipelineStateCache()
{
	return PipelineStateCache;
}

FSamplerStateCache& URenderer::GetSamplerStateCache()
{
	return SamplerStateCache;
}

FCommandListHandle URenderer::GetCommandList() const
{
	return CommandList;
}

FRenderViewport URenderer::GetViewport() const
{
	return RenderContext.GetViewport();
}

void URenderer::Create(void* NativeWindowHandle)
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

void URenderer::Release()
{
	checkf(!CommandList.IsValid(), "열린 Render Command List가 있는 상태에서 Renderer를 해제할 수 없다.");
	ReleaseAssetReferences();
	DefaultMaterialResource.Release();
	DebugDraw.Release();
	DefaultTextureResource.Release();
	SamplerStateCache.Release();
	PipelineStateCache.Release();
	ShaderRegistry.Release();
	RenderContext.Release();
	RenderDevice.Release();
}

void URenderer::ReleaseAssetReferences()
{
	checkf(!CommandList.IsValid(), "열린 Render Command List가 있는 상태에서 Asset 참조를 해제할 수 없다.");
	MaterialResources.clear();
	TextureResources.clear();
	StaticMeshResources.clear();
}

void URenderer::UpdateTextureResource(const FTextureResourceCommand& Command)
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

void URenderer::UpdateStaticMeshResource(const FStaticMeshResourceCommand& Command)
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

void URenderer::UpdateMaterialResource(const FMaterialResourceCommand& Command)
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

FTextureResource* URenderer::FindTextureResource(const FAssetId& AssetId) const
{
	const auto Iterator = TextureResources.find(AssetId);
	return Iterator != TextureResources.end() ? Iterator->second.get() : nullptr;
}

FStaticMeshResource* URenderer::FindStaticMeshResource(const FAssetId& AssetId) const
{
	const auto Iterator = StaticMeshResources.find(AssetId);
	return Iterator != StaticMeshResources.end() ? Iterator->second.get() : nullptr;
}

FMaterialResource* URenderer::FindMaterialResource(const FAssetId& AssetId) const
{
	const auto Iterator = MaterialResources.find(AssetId);
	return Iterator != MaterialResources.end() ? Iterator->second.get() : nullptr;
}

void URenderer::Resize(uint32 Width, uint32 Height)
{
	checkf(!CommandList.IsValid(), "열린 Render Command List가 있는 상태에서 Render Context 크기를 변경할 수 없다.");
	RenderContext.Resize(Width, Height);
}

void URenderer::BeginFrame()
{
	KNOT_PROFILE_SCOPE("Render", "URenderer::BeginFrame");

	checkf(!CommandList.IsValid(), "Renderer Frame이 이미 시작되었다.");
	CommandList = RenderDevice.BeginCommandList();
	RenderDevice.BeginFrameStatistics(CommandList);
	RenderContext.BeginFrame(CommandList);
}

void URenderer::EndFrame()
{
	KNOT_PROFILE_SCOPE("Render", "URenderer::EndFrame");

	checkf(CommandList.IsValid(), "Renderer Frame이 시작되지 않았다.");
	RenderContext.EndFrame(CommandList);
	RenderDevice.EndFrameStatistics(CommandList);
	RenderDevice.EndCommandList(CommandList);
	RenderDevice.Submit(CommandList);
	RenderContext.Present();
	DebugDraw.Reset();
}

void URenderer::Execute(FRenderGraph& RenderGraph)
{
	check(CommandList.IsValid());
	RenderGraph.Execute();
}

// World를 offscreen Color/Depth Target에 렌더링하도록 출력 대상과 Viewport를 설정하고 이전 프레임의 내용을 초기화한다.
void URenderer::BeginRenderTarget(FTextureHandle ColorTarget, FTextureHandle DepthTarget, const FRenderViewport& Viewport)
{
	check(CommandList.IsValid());
	static constexpr float ViewportClearColor[4] = { 0.035f, 0.04f, 0.05f, 1.0f };
	RenderDevice.SetRenderTargets(CommandList, ColorTarget, DepthTarget);
	RenderDevice.SetViewport(CommandList, Viewport);
	RenderDevice.ClearRenderTarget(CommandList, ColorTarget, ViewportClearColor);
	RenderDevice.ClearDepthStencil(CommandList, DepthTarget, 0.0f, 0);
}

// Offscreen RTV 바인딩을 끝내고 Back Buffer를 복구하여 이후 ImGui가 Color Target의 SRV를 화면에 렌더링할 수 있게 한다.
void URenderer::EndRenderTarget()
{
	check(CommandList.IsValid());
	RenderContext.BindBackBuffer(CommandList);
}
