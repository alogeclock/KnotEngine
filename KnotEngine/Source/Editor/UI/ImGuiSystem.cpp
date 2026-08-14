#include "ImGuiSystem.h"

#include "Core/IO/Paths.h"
#include "Render/Renderer.h"

#include <filesystem>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

void FImGuiSystem::Startup(HWND WindowHandle, URenderer& Renderer)
{
    ImGui::CreateContext();

    std::filesystem::create_directories(FPaths::SettingDir());
    ImGuiSettingsPath = FPaths::ToUtf8(FPaths::ImGuiSettingsPath());
    ImGui::GetIO().IniFilename = ImGuiSettingsPath.c_str();

    ImGui_ImplWin32_Init(WindowHandle);
    ImGui_ImplDX11_Init(Renderer.GetDevice(), Renderer.GetDeviceContext());
}

void FImGuiSystem::BeginFrame()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void FImGuiSystem::EndFrame()
{
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void FImGuiSystem::Shutdown()
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}
