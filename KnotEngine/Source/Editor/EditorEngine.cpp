#include "EditorEngine.h"

#include "Components/CubeComponent.h"

UEditorEngine::UEditorEngine()
    : Renderer(RenderBackend)
    , ImGuiRenderBackend(RenderBackend)
    , EditorUISystem(ImGuiRenderBackend)
{
}

void UEditorEngine::Startup(FWindowsWindow InWindow)
{
    Renderer.Create(InWindow.GetHwnd());
    EditorUISystem.Startup(InWindow.GetHwnd());
    Cube = new UCubeComponent(Renderer);
}

void UEditorEngine::Tick(float DeltaTime)
{
    Renderer.Prepare();
    Cube->Render(DeltaTime, Renderer);

    EditorUISystem.BeginFrame();
    EditorUISystem.Draw();
    EditorUISystem.EndFrame();

    Renderer.SwapBuffer();
}

void UEditorEngine::Shutdown()
{
    if (Cube)
    {
        delete Cube;
        Cube = nullptr;
    }

    EditorUISystem.Shutdown();
    Renderer.Release();
}
