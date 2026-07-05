#include "EditorEngine.h"

void UEditorEngine::Startup(FWindowsWindow InWindow)
{
	Renderer.Create(InWindow.GetHwnd());
	ImGuiSystem.Startup(InWindow.GetHwnd(), Renderer);
};

void UEditorEngine::Tick(float DeltaTime)
{
	Renderer.Prepare();

	ImGuiSystem.BeginFrame();
	EditorUI.Draw(DeltaTime);
	ImGuiSystem.EndFrame();

	Renderer.SwapBuffer();
};

void UEditorEngine::Shutdown()
{
	ImGuiSystem.Shutdown();
	Renderer.Release();
};
