#include "EditorEngine.h"

#include "Components/CubeComponent.h"
#include "Core/Assert.h"

UEditorEngine::UEditorEngine()
	: RenderContext(RenderDevice), Renderer(RenderDevice, RenderContext),
	  ImGuiRenderBackend(RenderDevice), EditorUISystem(ImGuiRenderBackend, EditorInputRouter)
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

void UEditorEngine::ProcessInput(const FInputSnapshot& InputSnapshot)
{
	EditorInputRouter.BeginFrame(InputSnapshot);
}

void UEditorEngine::Tick(float DeltaTime)
{
	check(Cube);

	Renderer.BeginFrame();

	EditorUISystem.BeginFrame();
	EditorUISystem.Draw(DeltaTime);
	EditorInputRouter.RouteInput();

	Cube->Render(DeltaTime, Renderer);
	EditorUISystem.EndFrame();

	Renderer.EndFrame();
}

void UEditorEngine::Shutdown()
{
	check(Cube);

	delete Cube;
	Cube = nullptr;

	EditorInputRouter.Reset();
	EditorUISystem.Shutdown();
	Renderer.Release();
}
