#include "EditorEngine.h"

#include "Components/CubeComponent.h"
#include "Core/Assert.h"

UEditorEngine::UEditorEngine()
	: RenderContext(RenderDevice), Renderer(RenderDevice, RenderContext),
	  ImGuiRenderBackend(RenderDevice), EditorUISystem(ImGuiRenderBackend, InputRouter)
{
}

void UEditorEngine::Startup(FWindowsWindow InWindow)
{
	check(!Cube);
	checkf(InWindow.GetHwnd(), "창 생성이 끝나기 전에 UEditorEngine::Startup() 호출.");

	Renderer.Create(InWindow.GetHwnd());
	EditorUISystem.Startup(InWindow.GetHwnd());
	Cube = GUObjectManager.Create<UCubeComponent>(Renderer);
}

void UEditorEngine::ProcessInput(const FInputSnapshot& InputSnapshot)
{
	InputRouter.BeginFrame(InputSnapshot);
}

void UEditorEngine::Tick(float DeltaTime)
{
	check(Cube);

	Renderer.BeginFrame();

	EditorUISystem.BeginFrame();
	EditorUISystem.Draw(DeltaTime);
	InputRouter.RouteInput();

	Cube->Render(DeltaTime, Renderer);
	EditorUISystem.EndFrame(Renderer.GetCommandList());

	Renderer.EndFrame();
}

void UEditorEngine::Shutdown()
{
	check(Cube);

	GUObjectManager.Destroy(Cube);
	Cube = nullptr;

	InputRouter.Reset();
	EditorUISystem.Shutdown();
	Renderer.Release();
}
