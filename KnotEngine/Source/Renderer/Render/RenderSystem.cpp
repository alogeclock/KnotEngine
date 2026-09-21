#include "Render/RenderSystem.h"

#include "Asset/AssetManager.h"
#include "Core/Assert.h"
#include "Core/Profiling/CPUProfiler.h"
#include "Render/ImGui/ImGuiRenderBackend.h"
#include "Render/RenderBackend.h"
#include "Render/Renderer.h"
#include "Render/RHI/RenderDevice.h"
#include "Render/Resource/ResourceCommand.h"
#include "Render/Scene/Scene.h"
#include "Render/Scene/SceneRenderer.h"

#include <algorithm>

FRenderSystem::FRenderSystem()
	: RenderBackend(CreateRenderBackend()),
	  Renderer(std::make_unique<FRenderer>(RenderBackend->GetRenderDevice(), RenderBackend->GetRenderContext(), RenderBackend->GetShaderFormat()))
{
}

FRenderSystem::~FRenderSystem()
{
	Shutdown();
}

void FRenderSystem::Startup(void* NativeWindowHandle)
{
	check(!bStarted && NativeWindowHandle);
	RenderThread.Startup();
	RenderThread.EnqueueAndWait([this, NativeWindowHandle] { Renderer->Create(NativeWindowHandle); });
	bStarted = true;
}

void FRenderSystem::Shutdown()
{
	if (!bStarted)
	{
		return;
	}
	RenderThread.EnqueueAndWait([this] { Renderer->Release(); });
	RenderThread.Shutdown();
	bStarted = false;
}

void FRenderSystem::Flush()
{
	RenderThread.Flush();
}

// World를 파괴하기 직전에 발생하는 동기 실행, 렌더링 시에는 FScene 파라미터 없는 Flush() 실행.
void FRenderSystem::Flush(FScene& Scene)
{
	auto Commands = std::make_shared<TArray<FPrimitiveRenderCommand>>(Scene.DrainRenderCommands());
	RenderThread.EnqueueAndWait([this, &Scene, Commands] { Scene.ApplyRenderCommands(*Renderer, std::move(*Commands)); });
}

void FRenderSystem::ResizeWindow(uint32 Width, uint32 Height)
{
	RenderThread.EnqueueAndWait([this, Width, Height] { Renderer->Resize(Width, Height); });
}

void FRenderSystem::Render(TArray<FScene*>&& Scenes, TArray<FSceneViewFamily>&& ViewFamilies, FImGuiDrawDataCopy&& ImGuiDrawData)
{
	uint64 FrameNumber = 0;
	{
		std::unique_lock Lock(FrameMutex);
		FrameCondition.wait(Lock, [this] { return SubmittedRenderFrames - CompletedRenderFrames < MaxFramesInFlight; });
		FrameNumber = ++SubmittedRenderFrames;
	}

	auto StaticMeshCommands = std::make_shared<TArray<FStaticMeshResourceCommand>>();
	auto TextureCommands = std::make_shared<TArray<FTextureResourceCommand>>();
	auto MaterialCommands = std::make_shared<TArray<FMaterialResourceCommand>>();

	check(GAssetManager);
	GAssetManager->DrainRenderResourceCommands(*StaticMeshCommands, *TextureCommands, *MaterialCommands);

	auto SceneBatches = std::make_shared<TArray<FSceneCommandBatch>>();
	SceneBatches->reserve(Scenes.size());
	for (FScene* Scene : Scenes)
	{
		check(Scene);
		SceneBatches->push_back({ Scene, Scene->DrainRenderCommands() });
	}
	auto Families = std::make_shared<TArray<FSceneViewFamily>>(std::move(ViewFamilies));
	auto DrawData = std::make_shared<FImGuiDrawDataCopy>(std::move(ImGuiDrawData));
	RenderThread.Enqueue([this, FrameNumber, StaticMeshCommands, TextureCommands, MaterialCommands, SceneBatches, Families, DrawData]
	{
#if KNOT_CPU_PROFILER_ENABLED
		FCPUProfiler::BeginFrame(ECPUProfileThread::Render, 0.0f);
#endif
		for (const FTextureResourceCommand& Command : *TextureCommands)
		{
			Renderer->UpdateTextureResource(Command);
		}

		for (const FStaticMeshResourceCommand& Command : *StaticMeshCommands)
		{
			Renderer->UpdateStaticMeshResource(Command);
		}

		for (const FMaterialResourceCommand& Command : *MaterialCommands)
		{
			Renderer->UpdateMaterialResource(Command);
		}

		for (FSceneCommandBatch& Batch : *SceneBatches)
		{
			Batch.Scene->ApplyRenderCommands(*Renderer, std::move(Batch.Commands));
		}

		const FRenderViewport OutputViewport = Renderer->GetViewport();
		if (OutputViewport.Width > 0.0f && OutputViewport.Height > 0.0f)
		{
			Renderer->BeginFrame();
			for (const FSceneViewFamily& Family : *Families)
			{
				FSceneRenderer SceneRenderer(Family);
				SceneRenderer.Render(*Renderer);
			}
			if (DrawData->IsVisible())
			{
				RenderBackend->GetImGuiRenderBackend().Render(Renderer->GetCommandList(), *DrawData);
			}
			Renderer->EndFrame();
			FInstancedDrawStatistics InstancedDrawStatistics = Renderer->GetInstancedDrawStatistics();
			InstancedDrawStatistics.FrameNumber = FrameNumber;
			InstancedDrawStatistics.bValid = true;
			std::lock_guard Lock(FrameStatisticsMutex);
			LastGPUFrameStatistics = RenderBackend->GetRenderDevice().GetLastFrameStatistics();
			LastInstancedDrawStatistics = InstancedDrawStatistics;
		}

#if KNOT_CPU_PROFILER_ENABLED
		FCPUProfiler::EndFrame();
#endif
		{
			std::lock_guard Lock(FrameMutex);
			CompletedRenderFrames = FrameNumber;
		}
		FrameCondition.notify_one();
	});
}

FTextureHandle FRenderSystem::CreateTexture(const FTextureDesc& Desc, std::span<const FTextureSubresourceData> InitialData)
{
	FTextureHandle Result;
	RenderThread.EnqueueAndWait([this, &Result, &Desc, InitialData] { Result = RenderBackend->GetRenderDevice().CreateTexture(Desc, InitialData); });
	return Result;
}

void FRenderSystem::DestroyTexture(FTextureHandle& Texture)
{
	if (!Texture.IsValid())
	{
		return;
	}
	FTextureHandle ReleasedTexture = Texture;
	Texture.Reset();
	RenderThread.Enqueue([this, ReleasedTexture]() mutable { RenderBackend->GetRenderDevice().DestroyTexture(ReleasedTexture); });
}

FViewportRenderTargets FRenderSystem::ResizeViewportTargets(FViewportRenderTargets&& CurrentTargets, uint32 Width, uint32 Height)
{
	FViewportRenderTargets Result;
	RenderThread.EnqueueAndWait([this, Width, Height, CurrentTargets = std::move(CurrentTargets), &Result]() mutable
	{
		IRenderDevice& RenderDevice = RenderBackend->GetRenderDevice();
		RenderDevice.DestroyTexture(CurrentTargets.RenderTarget.Depth);
		RenderDevice.DestroyTexture(CurrentTargets.RenderTarget.SelectionDepth);
		RenderDevice.DestroyTexture(CurrentTargets.RenderTarget.DisplayColor);
		RenderDevice.DestroyTexture(CurrentTargets.RenderTarget.SceneColor);

		FTextureDesc ColorDesc;
		ColorDesc.Width = Width;
		ColorDesc.Height = Height;
		ColorDesc.Format = ETextureFormat::BGRA8UNorm;
		ColorDesc.Usage = ETextureUsage::RenderTarget | ETextureUsage::ShaderResource;
		Result.RenderTarget.SceneColor = RenderDevice.CreateTexture(ColorDesc);
		Result.RenderTarget.DisplayColor = RenderDevice.CreateTexture(ColorDesc);

		FTextureDesc DepthDesc;
		DepthDesc.Width = Width;
		DepthDesc.Height = Height;
		DepthDesc.Format = ETextureFormat::D32Float;
		DepthDesc.Usage = ETextureUsage::DepthStencil | ETextureUsage::ShaderResource;
		Result.RenderTarget.SelectionDepth = RenderDevice.CreateTexture(DepthDesc);
		DepthDesc.Usage = ETextureUsage::DepthStencil;
		Result.RenderTarget.Depth = RenderDevice.CreateTexture(DepthDesc);
		Result.RenderTarget.Width = Width;
		Result.RenderTarget.Height = Height;
		Result.DisplayTextureId = RenderBackend->GetImGuiRenderBackend().GetImGuiTextureID(Result.RenderTarget.DisplayColor);
	});
	return Result;
}

void FRenderSystem::ReleaseViewportTargets(FViewportRenderTargets& Targets)
{
	if (!Targets.RenderTarget.SceneColor.IsValid() && !Targets.RenderTarget.DisplayColor.IsValid() &&
		!Targets.RenderTarget.SelectionDepth.IsValid() && !Targets.RenderTarget.Depth.IsValid())
	{
		return;
	}
	FViewportRenderTargets ReleasedTargets = Targets;
	Targets = {};
	RenderThread.Enqueue([this, ReleasedTargets]() mutable
	{
		IRenderDevice& RenderDevice = RenderBackend->GetRenderDevice();
		RenderDevice.DestroyTexture(ReleasedTargets.RenderTarget.Depth);
		RenderDevice.DestroyTexture(ReleasedTargets.RenderTarget.SelectionDepth);
		RenderDevice.DestroyTexture(ReleasedTargets.RenderTarget.DisplayColor);
		RenderDevice.DestroyTexture(ReleasedTargets.RenderTarget.SceneColor);
	});
}

ImTextureID FRenderSystem::GetImGuiTextureID(FTextureHandle Texture)
{
	if (!Texture.IsValid())
	{
		return {};
	}
	ImTextureID Result = {};
	RenderThread.EnqueueAndWait([this, Texture, &Result] { Result = RenderBackend->GetImGuiRenderBackend().GetImGuiTextureID(Texture); });
	return Result;
}

FGPUFrameStatistics FRenderSystem::GetLastGPUFrameStatistics() const
{
	std::lock_guard Lock(FrameStatisticsMutex);
	return LastGPUFrameStatistics;
}

FInstancedDrawStatistics FRenderSystem::GetLastInstancedDrawStatistics() const
{
	std::lock_guard Lock(FrameStatisticsMutex);
	return LastInstancedDrawStatistics;
}

ImTextureID FRenderSystem::StartupImGui(std::span<const uint8> FontPixels, uint32 FontWidth, uint32 FontHeight)
{
	ImTextureID FontTextureId = {};
	RenderThread.EnqueueAndWait([this, FontPixels, FontWidth, FontHeight, &FontTextureId]
	{
		FontTextureId = RenderBackend->GetImGuiRenderBackend().Startup(FontPixels, FontWidth, FontHeight);
	});
	return FontTextureId;
}

void FRenderSystem::ShutdownImGui()
{
	RenderThread.EnqueueAndWait([this] { RenderBackend->GetImGuiRenderBackend().Shutdown(); });
}

void FRenderSystem::ReleaseAssetResources()
{
	RenderThread.EnqueueAndWait([this] { Renderer->ReleaseAssetReferences(); });
	if (GAssetManager)
	{
		GAssetManager->ResetRenderResourceRequests();
	}
}
