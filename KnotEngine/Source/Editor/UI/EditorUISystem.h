#pragma once

#include <Windows.h>

class IImGuiRenderBackend;

class FEditorUISystem
{
public:
    explicit FEditorUISystem(IImGuiRenderBackend& InRenderBackend);

    void Startup(HWND WindowHandle);
    void BeginFrame();
    void Draw();
    void EndFrame();
    void Shutdown();

private:
    IImGuiRenderBackend& RenderBackend;
    float ClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f };
};
