#include "Render/RenderSystem.h"

#include "Asset/AssetManager.h"
#include "Core/Assert.h"
#include "Core/IO/Paths.h"
#include "Core/Log.h"
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

// Renderer를 기동한 뒤 Content 디렉터리의 Shader 변경 감시를 시작한다.
void FRenderSystem::Startup(void* NativeWindowHandle)
{
	check(!bStarted && NativeWindowHandle);
	RenderThread.Startup();
	
	RenderThread.EnqueueAndWait([this, NativeWindowHandle]
	{
		Renderer->Create(NativeWindowHandle);
		PublishShaderSourcePaths();
	});
	
	if (!DirectoryWatcher.Start(FPaths::ContentDir()))
	{
		KE_LOG(LogRenderer, Error, "Content 디렉터리 변경 감시를 시작하지 못했다. Path={}", FPaths::ToUtf8(FPaths::ContentDir()));
	}
	
	bStarted = true;
}

// 파일 감시를 중지하고 Render Thread의 GPU 자원을 순서대로 해제한다.
void FRenderSystem::Shutdown()
{
	if (!bStarted)
	{
		return;
	}
	DirectoryWatcher.Stop();
	RenderThread.EnqueueAndWait([this] { Renderer->Release(); });
	RenderThread.Shutdown();
	{
		std::lock_guard Lock(ShaderSourcesMutex);
		ShaderSourcePaths.clear();
	}
	PublishedShaderCount = 0;
	bStarted = false;
}

void FRenderSystem::Flush()
{
	RenderThread.Flush();
}

// 변경된 Shader Key를 컴파일하고 Material 후보와 함께 RT에서 동기 교체한다.
bool FRenderSystem::ReloadShaders(const FString& SourcePath, FString& Diagnostics, std::span<const uint8> SourceSnapshot)
{
	check(bStarted);
	TArray<FShaderKey> Keys;
	
	RenderThread.EnqueueAndWait([this, &SourcePath, &Keys] { Keys = Renderer->GetShaderKeysForSource(SourcePath); });
	
	if (Keys.empty())
	{
		Diagnostics = SourcePath.empty() ? "등록된 Shader가 없다." : "이 파일을 사용하는 Shader Key가 없다: " + SourcePath;
		return false;
	}
	
	FShaderCompiler Compiler(RenderBackend->GetShaderFormat());
	TArray<FShaderReloadEntry> Compiled;
	TMap<FString, TArray<uint8>> Sources;
	Compiled.reserve(Keys.size());
	
	for (const FShaderKey& Key : Keys)
	{
		auto Source = Sources.find(Key.SourcePath);
		if (Source == Sources.end())
		{
			TArray<uint8> Bytes;
			if (!SourceSnapshot.empty() && Key.SourcePath == SourcePath)
			{
				Bytes.assign(SourceSnapshot.begin(), SourceSnapshot.end());
			}
			else if (!FShaderCompiler::TryLoadSource(Key, Bytes, Diagnostics))
			{
				return false;
			}
			Source = Sources.emplace(Key.SourcePath, std::move(Bytes)).first;
		}
		FShaderCompilerOutput Output;
		if (!Compiler.TryCompile(Key, Source->second, Output, Diagnostics))
		{
			return false;
		}
		Compiled.push_back({ Key, std::move(Output) });
	}
	
	check(GAssetManager);
	TArray<FMaterialResourceCommand> MaterialCommands;
	if (!GAssetManager->PrepareMaterialShaderReload(Keys, MaterialCommands))
	{
		Diagnostics = "Shader 변경 대상 Material의 렌더 데이터를 준비하지 못했다.";
		return false;
	}

	bool bSucceeded = false;
	RenderThread.EnqueueAndWait([this, &Compiled, &MaterialCommands, &Diagnostics, &bSucceeded]
	{
		bSucceeded = Renderer->ReloadShaders(Compiled, MaterialCommands, Diagnostics);
	});
	return bSucceeded;
}

// Render Thread가 발행한 Shader 소스 경로 목록을 잠금 아래 복사한다.
TArray<FString> FRenderSystem::GetShaderSourcePaths()
{
	std::lock_guard Lock(ShaderSourcesMutex);
	return ShaderSourcePaths;
}

// Directory Watcher가 실제 내용 변경을 확정한 Shader만 다시 컴파일한다.
void FRenderSystem::PollShaderChanges()
{
	for (const FString& SourcePath : GetShaderSourcePaths())
	{
		DirectoryWatcher.Watch(FPaths::ResolveContentPath(SourcePath));
	}
	for (FDirectoryWatcher::FChangedFile& Change : DirectoryWatcher.PollChanges())
	{
		const std::filesystem::path RelativePath = Change.Path.lexically_relative(FPaths::ContentDir());
		const FString SourcePath = "/" + FPaths::ToUtf8(RelativePath.generic_wstring());
		FString Diagnostics;
		if (!Change.Contents)
		{
			Diagnostics = "Shader 소스 파일을 읽을 수 없다.";
		}
		if (!Change.Contents || !ReloadShaders(SourcePath, Diagnostics, *Change.Contents))
		{
			KE_LOG(LogRenderer, Error, "Shader Hot Reload 실패. Path={}, Error={}", SourcePath, Diagnostics);
		}
		else
		{
			KE_LOG(LogRenderer, Display, "Shader Hot Reload 완료. Path={}", SourcePath);
		}
	}
}

// Registry에 새 Shader Key가 등록되면 감시할 소스 경로 목록을 갱신한다.
void FRenderSystem::PublishShaderSourcePaths()
{
	const SIZE_T Count = Renderer->GetShaderRegistry().GetEntryCount();
	if (Count == PublishedShaderCount)
	{
		return;
	}
	
	// 빈 경로는 전체 등록 Key를 뜻한다. 컴파일은 GT에서 끝내고 RT에서 새 GPU Shader를 준비한다.
	const TArray<FShaderKey> Keys = Renderer->GetShaderKeysForSource({});
	TArray<FString> Paths;
	for (const FShaderKey& Key : Keys)
	{
		if (std::find(Paths.begin(), Paths.end(), Key.SourcePath) == Paths.end())
		{
			Paths.push_back(Key.SourcePath);
		}
	}
	
	std::sort(Paths.begin(), Paths.end());
	{
		std::lock_guard Lock(ShaderSourcesMutex);
		ShaderSourcePaths = std::move(Paths);
	}
	
	PublishedShaderCount = Count;
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

// Shader 변경을 확인하고 Asset·Scene 명령과 UI 데이터를 한 프레임 작업으로 제출한다.
void FRenderSystem::Render(TArray<FScene*>&& Scenes, TArray<FSceneViewFamily>&& ViewFamilies, FImGuiDrawDataCopy&& ImGuiDrawData)
{
	PollShaderChanges();
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
		PublishShaderSourcePaths();

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
