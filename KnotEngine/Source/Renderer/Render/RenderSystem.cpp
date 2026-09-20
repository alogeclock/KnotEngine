#include "Render/RenderSystem.h"

#include "Asset/AssetManager.h"
#include "Core/Assert.h"
#include "Render/ImGui/ImGuiRenderBackend.h"
#include "Render/RenderBackend.h"
#include "Render/Renderer.h"
#include "Render/RHI/RenderDevice.h"
#include "Render/Resource/ResourceCommand.h"
#include "Render/Scene/Scene.h"
#include "Render/Scene/SceneRenderer.h"

#include <imgui.h>
#include <algorithm>

FRenderSystem::FRenderSystem()
	: RenderBackend(CreateRenderBackend()),
	  Renderer(std::make_unique<URenderer>(RenderBackend->GetRenderDevice(), RenderBackend->GetRenderContext(), RenderBackend->GetShaderFormat()))
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

void FRenderSystem::ResizeWindow(uint32 Width, uint32 Height)
{
	RenderThread.EnqueueAndWait([this, Width, Height] { Renderer->Resize(Width, Height); });
}

void FRenderSystem::Render(TArray<FScene*>&& Scenes, TArray<FSceneViewFamily>&& ViewFamilies, ImDrawData* DrawData)
{
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
	RenderThread.EnqueueAndWait([this, StaticMeshCommands, TextureCommands, MaterialCommands, SceneBatches, Families, DrawData]
	{
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
		if (OutputViewport.Width <= 0.0f || OutputViewport.Height <= 0.0f)
		{
			return;
		}
		Renderer->BeginFrame();
		for (const FSceneViewFamily& Family : *Families)
		{
			FSceneRenderer SceneRenderer(Family);
			SceneRenderer.Render(*Renderer);
		}
		if (DrawData)
		{
			RenderBackend->GetImGuiRenderBackend().Render(Renderer->GetCommandList(), DrawData);
		}
		Renderer->EndFrame();
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
	RenderThread.EnqueueAndWait([this, &Texture] { RenderBackend->GetRenderDevice().DestroyTexture(Texture); });
}

ImTextureID FRenderSystem::GetImGuiTextureID(FTextureHandle Texture)
{
	ImTextureID Result = {};
	RenderThread.EnqueueAndWait([this, Texture, &Result] { Result = RenderBackend->GetImGuiRenderBackend().GetImGuiTextureID(Texture); });
	return Result;
}

FGPUFrameStatistics FRenderSystem::GetLastGPUFrameStatistics()
{
	FGPUFrameStatistics Result;
	RenderThread.EnqueueAndWait([this, &Result] { Result = RenderBackend->GetRenderDevice().GetLastFrameStatistics(); });
	return Result;
}

void FRenderSystem::StartupImGui(ImGuiContext* Context)
{
	RenderThread.EnqueueAndWait([this, Context] { RenderBackend->GetImGuiRenderBackend().Startup(Context); });
}

void FRenderSystem::BeginImGuiFrame()
{
	RenderThread.EnqueueAndWait([this] { RenderBackend->GetImGuiRenderBackend().BeginFrame(); });
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
