#include "EditorEngine.h"

#include "Components/CubeComponent.h"
#include "Core/Assert.h"

UEditorEngine::UEditorEngine()
	: Renderer(RenderBackend), ImGuiRenderBackend(RenderBackend), EditorUISystem(ImGuiRenderBackend)
{
}

void UEditorEngine::Startup(FWindowsWindow InWindow)
{
	check(!Cube);
	checkf(InWindow.GetHwnd(), "창 생성이 끝나기 전에 UEditorEngine::Startup() 호출.");

	Renderer.Create(InWindow.GetHwnd());
	EditorUISystem.Startup(InWindow.GetHwnd());
	Cube = new UCubeComponent(Renderer);
}

void UEditorEngine::Tick(float DeltaTime)
{
	check(Cube);

	Renderer.Prepare();
	Cube->Render(DeltaTime, Renderer);

	EditorUISystem.BeginFrame();
	EditorUISystem.Draw(DeltaTime);
	EditorUISystem.EndFrame();

	Renderer.SwapBuffer();
}

void UEditorEngine::Shutdown()
{
	check(Cube);

	delete Cube;
	Cube = nullptr;

	EditorUISystem.Shutdown();
	Renderer.Release();
}
