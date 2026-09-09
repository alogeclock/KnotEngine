#pragma once

#include "Render/D3D11/D3D11RenderContext.h"
#include "Render/D3D11/D3D11RenderDevice.h"
#include "Render/ImGui/D3D11ImGuiBackend.h"
#include "Render/Renderer.h"
#include "Runtime/Engine.h"
#include "Input/InputRouter.h"
#include "UI/ImGuiSystem.h"
#include "Viewport/LevelEditorViewportClient.h"
#include "Viewport/Viewport.h"

UCLASS()
class UEditorEngine : public UEngine
{
	GENERATED_CLASS(UEditorEngine, UEngine)

public:
	UEditorEngine();
	~UEditorEngine() override = default;

	// Editor 모듈 전체의 리플렉션 등록/해제 진입점. 구현은 Reflection.gen.cpp에서 생성한다.
	static void RegisterTypes(FReflectionRegistry& Registry);
	static void ResetTypes();

	void Startup(FWindowsWindow InWindow) override;
	void ProcessInput(const FInputSnapshot& InputSnapshot) override;
	void OnWindowResized(FWindowSize Size) override;
	void Tick(float DeltaTime) override;
	void Shutdown() override;

private:
	void Render(UWorld& World);

	URenderer Renderer;

	// TO-DO: EditorEngine는 Editor 전용이므로 추상화된 IRenderDevice와 IRenderContext를 사용하도록 한다.
	FD3D11RenderDevice RenderDevice;
	FD3D11RenderContext RenderContext;
	FD3D11ImGuiBackend ImGuiRenderBackend;

	FViewport LevelViewport;
	FLevelEditorViewportClient LevelViewportClient;

	FInputRouter InputRouter;
	FImGuiSystem ImGuiSystem;

	uint64 EditorContextId = 0;
};
