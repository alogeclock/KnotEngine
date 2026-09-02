#pragma once

#include "Render/D3D11/D3D11RenderContext.h"
#include "Render/D3D11/D3D11RenderDevice.h"
#include "Render/Renderer.h"
#include "Runtime/Engine.h"
#include "Input/InputRouter.h"
#include "UI/EditorUISystem.h"
#include "UI/ImGui/D3D11ImGuiBackend.h"

class UCubeComponent;

class UEditorEngine : public UEngine
{
public:
	UEditorEngine();
	~UEditorEngine() override = default;

	void Startup(FWindowsWindow InWindow) override;
	void ProcessInput(const FInputSnapshot& InputSnapshot) override;
	void Tick(float DeltaTime) override;
	void Shutdown() override;

private:
	FD3D11RenderDevice RenderDevice;
	FD3D11RenderContext RenderContext;
	URenderer Renderer;
	FD3D11ImGuiBackend ImGuiRenderBackend;

	FInputRouter InputRouter;
	FEditorUISystem EditorUISystem;

	UCubeComponent* Cube = nullptr;
};
