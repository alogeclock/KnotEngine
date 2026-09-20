#pragma once

#include "RendererAPI.h"
#include "Render/RenderThread.h"
#include "Render/RHI/RenderTypes.h"
#include "Render/Scene/Scene.h"
#include "Render/Scene/SceneView.h"

#include <memory>
#include <span>
#include <imgui.h>

class IRenderBackend;
class URenderer;
struct ImDrawData;
struct ImGuiContext;

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

	void ResizeWindow(uint32 Width, uint32 Height);
	void Render(TArray<FScene*>&& Scenes, TArray<FSceneViewFamily>&& ViewFamilies, ImDrawData* DrawData);

	FTextureHandle CreateTexture(const FTextureDesc& Desc, std::span<const FTextureSubresourceData> InitialData = {});
	void DestroyTexture(FTextureHandle& Texture);

	ImTextureID GetImGuiTextureID(FTextureHandle Texture);
	FGPUFrameStatistics GetLastGPUFrameStatistics();

	void StartupImGui(ImGuiContext* Context);
	void BeginImGuiFrame();
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
	bool bStarted = false;
};
