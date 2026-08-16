#include "UI/ImGui/D3D11ImGuiBackend.h"

#include "Render/Backends/D3D11Backend.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>

FD3D11ImGuiBackend::FD3D11ImGuiBackend(FD3D11Backend& InBackend)
    : Backend(InBackend)
{
}

void FD3D11ImGuiBackend::Startup()
{
    ImGui_ImplDX11_Init(Backend.GetNativeDevice(), Backend.GetNativeContext());
}

void FD3D11ImGuiBackend::BeginFrame()
{
    ImGui_ImplDX11_NewFrame();
}

void FD3D11ImGuiBackend::Render(ImDrawData* DrawData)
{
    ImGui_ImplDX11_RenderDrawData(DrawData);
}

void FD3D11ImGuiBackend::Shutdown()
{
    ImGui_ImplDX11_Shutdown();
}
