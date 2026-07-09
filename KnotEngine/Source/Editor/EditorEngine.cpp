#include "EditorEngine.h"
#include "Components/CubeComponent.h"

void UEditorEngine::Startup(FWindowsWindow InWindow)
{
	Renderer.Create(InWindow.GetHwnd());
	ImGuiSystem.Startup(InWindow.GetHwnd(), Renderer);
	Cube = new UCubeComponent(Renderer);
};

void UEditorEngine::Tick(float DeltaTime)
{
	Renderer.Prepare();
	Cube->Render(DeltaTime, Renderer);

	ImGuiSystem.BeginFrame();
	EditorUI.Draw(DeltaTime);
	ImGuiSystem.EndFrame();

	Renderer.SwapBuffer();
};

void UEditorEngine::Shutdown()
{
	if (Cube)
	{
		delete Cube;
		Cube = nullptr;
	}

	ImGuiSystem.Shutdown();
	Renderer.Release();
};
