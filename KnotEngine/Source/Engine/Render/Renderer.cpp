#include "Render/Renderer.h"

#include "Core/Assert.h"
#include "Core/Profiling/CPUProfiler.h"
#include "Render/Graph/RenderGraph.h"
#include "Render/Proxy/MaterialRenderProxy.h"
#include "Render/RHI/RenderContext.h"
#include "Render/RHI/RenderDevice.h"

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
	panicf(DefaultTexture.Initialize(RenderDevice, WhiteTextureDesc, std::span(&WhiteTextureData, 1)), "기본 White Texture 생성에 실패했다.");
	DebugDraw.Create();
}

void URenderer::Release()
{
	checkf(!CommandList.IsValid(), "열린 Render Command List가 있는 상태에서 Renderer를 해제할 수 없다.");
	MaterialRenderProxies.clear();
	DebugDraw.Release();
	DefaultTexture.Release();
	SamplerStateCache.Release();
	PipelineStateCache.Release();
	ShaderRegistry.Release();
	RenderContext.Release();
	RenderDevice.Release();
}

// 같은 Material Interface는 Renderer 수명 동안 하나의 Render Proxy로 등록해 재사용한다. nullptr는 기본 Material을 나타낸다.
FMaterialRenderProxy& URenderer::RegisterMaterial(const UMaterialInterface* MaterialInterface)
{
	const auto Existing = MaterialRenderProxies.find(MaterialInterface);
	if (Existing != MaterialRenderProxies.end())
	{
		return *Existing->second;
	}

	panicf(MaterialRenderProxies.size() < (std::numeric_limits<uint32>::max)(), "Material Render Proxy Sort ID가 uint32 범위를 초과했다.");
	auto Proxy = std::make_unique<FMaterialRenderProxy>(MaterialInterface, static_cast<uint32>(MaterialRenderProxies.size()));
	Proxy->Register(*this);
	FMaterialRenderProxy& Result = *Proxy;
	MaterialRenderProxies.emplace(MaterialInterface, std::move(Proxy));
	return Result;
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
	RenderContext.BeginFrame(CommandList);
}

void URenderer::EndFrame()
{
	KNOT_PROFILE_SCOPE("Render", "URenderer::EndFrame");

	checkf(CommandList.IsValid(), "Renderer Frame이 시작되지 않았다.");
	RenderContext.EndFrame(CommandList);
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
