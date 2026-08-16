#include "UI/EditorUISystem.h"

#include "Core/Assert.h"
#include "Core/IO/Paths.h"
#include "UI/ImGui/ImGuiRenderBackend.h"

#include <filesystem>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <string>
#include <system_error>

FEditorUISystem::FEditorUISystem(IImGuiRenderBackend& InRenderBackend)
	: RenderBackend(InRenderBackend)
{
}

void FEditorUISystem::Startup(HWND WindowHandle)
{
	checkf(WindowHandle, "HWND 생성 실패.");
	panicf(ImGui::CreateContext(), "ImGui Context 생성 실패.");

	std::error_code FileSystemError;
	std::filesystem::create_directories(FPaths::SettingDir(), FileSystemError);
	panicf(!FileSystemError, "ImGui 설정 디렉터리 생성 실패. Error={}", FileSystemError.message());

	static const std::string ImGuiSettingsPath = FPaths::ToUtf8(FPaths::ImGuiSettingsPath());
	ImGui::GetIO().IniFilename = ImGuiSettingsPath.c_str();

	panicf(ImGui_ImplWin32_Init(WindowHandle), "ImGui Win32 플랫폼 백엔드 초기화 실패.");

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
