#include "UI/EditorUISystem.h"

#include "Core/IO/Paths.h"
#include "UI/ImGui/ImGuiRenderBackend.h"

#include <filesystem>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <string>

FEditorUISystem::FEditorUISystem(IImGuiRenderBackend& InRenderBackend)
    : RenderBackend(InRenderBackend)
{
}

void FEditorUISystem::Startup(HWND WindowHandle)
{
    ImGui::CreateContext();

    std::filesystem::create_directories(FPaths::SettingDir());
    static const std::string ImGuiSettingsPath = FPaths::ToUtf8(FPaths::ImGuiSettingsPath());
    ImGui::GetIO().IniFilename = ImGuiSettingsPath.c_str();

    ImGui_ImplWin32_Init(WindowHandle);
    RenderBackend.Startup();
}

void FEditorUISystem::BeginFrame()
{
    RenderBackend.BeginFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void FEditorUISystem::Draw()
{
    ImGui::Begin("Knot Engine Property Window");
    ImGui::Text("Hello, Knot Engine!");
    ImGui::Separator();
    ImGui::ColorEdit4("Background Color", ClearColor);
    ImGui::End();
}

void FEditorUISystem::EndFrame()
{
    ImGui::Render();
    RenderBackend.Render(ImGui::GetDrawData());
}

void FEditorUISystem::Shutdown()
{
    RenderBackend.Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}
