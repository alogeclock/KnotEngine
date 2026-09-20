#pragma once

#include "RendererAPI.h"
#include "Render/ImGui/ImGuiDrawDataCopy.h"
#include "Render/RenderThread.h"
#include "Render/RHI/RenderTypes.h"
#include "Render/Scene/Scene.h"
#include "Render/Scene/SceneView.h"

#include <condition_variable>
#include <memory>
#include <imgui.h>
#include <mutex>
#include <span>

class IRenderBackend;
class URenderer;
struct RENDERER_API FViewportRenderTargets
{
	FSceneRenderTarget RenderTarget;
	ImTextureID DisplayTextureId = {};
};

// Render Backend, Renderer와 전용 Render Thread를 소유하는 프로세스 단위 렌더 진입점이다.
class RENDERER_API FRenderSystem
{
public:
	FRenderSystem();
	~FRenderSystem();

	FRenderSystem(const FRenderSystem&) = delete;
	FRenderSystem& operator=(const FRenderSystem&) = delete;

	void Startup(void* NativeWindowHandle);
	void Shutdown();

	void Flush();
	void Flush(FScene& Scene);

	void ResizeWindow(uint32 Width, uint32 Height);
	void Render(TArray<FScene*>&& Scenes, TArray<FSceneViewFamily>&& ViewFamilies, FImGuiDrawDataCopy&& ImGuiDrawData);

	FTextureHandle CreateTexture(const FTextureDesc& Desc, std::span<const FTextureSubresourceData> InitialData = {});
	void DestroyTexture(FTextureHandle& Texture);
	FViewportRenderTargets ResizeViewportTargets(FViewportRenderTargets&& CurrentTargets, uint32 Width, uint32 Height);
	void ReleaseViewportTargets(FViewportRenderTargets& Targets);

	ImTextureID GetImGuiTextureID(FTextureHandle Texture);
	FGPUFrameStatistics GetLastGPUFrameStatistics() const;

	ImTextureID StartupImGui(std::span<const uint8> FontPixels, uint32 FontWidth, uint32 FontHeight);
	void ShutdownImGui();

	void ReleaseAssetResources();

private:
	struct FSceneCommandBatch
	{
		FScene* Scene = nullptr;
		TArray<FPrimitiveRenderCommand> Commands;
	};

	std::unique_ptr<IRenderBackend> RenderBackend;
	std::unique_ptr<URenderer> Renderer;
	FRenderThread RenderThread;

	std::mutex FrameMutex;
	std::condition_variable FrameCondition;
	uint64 SubmittedRenderFrames = 0;
	uint64 CompletedRenderFrames = 0;
	static constexpr uint64 MaxFramesInFlight = 2;

	mutable std::mutex GPUStatisticsMutex;
	FGPUFrameStatistics LastGPUFrameStatistics;

	bool bStarted = false;
};
