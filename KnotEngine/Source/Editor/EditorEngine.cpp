#include "EditorEngine.h"

#include <filesystem>

namespace
{
	constexpr const char* ImGuiSettingsPath = "Settings/imgui.ini";

	void ConfigureImGuiSettingsPath()
	{
		std::error_code Error;
		std::filesystem::create_directories("Settings", Error);
		ImGui::GetIO().IniFilename = ImGuiSettingsPath;
	}
}

void UEditorEngine::Init(FWindowsWindow InWindow)
{
	Renderer.Create(InWindow.GetHwnd());

	ImGui::CreateContext();
	ConfigureImGuiSettingsPath();
	ImGui_ImplWin32_Init(InWindow.GetHwnd());
	ImGui_ImplDX11_Init(Renderer.GetDevice(), Renderer.GetDeviceContext());
};

void UEditorEngine::Tick(float DeltaTime)
{
	Renderer.Prepare();

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// Editor UI Drawing;
	ImGui::Begin("Knot Engine Property Window");

	ImGui::Text("Hello, Knot Engine!");
	ImGui::Separator();
	static float clear_color[4] = { 0.025f, 0.025f, 0.025f, 1.0f };
	ImGui::ColorEdit4("Background Color", clear_color);

	ImGui::End();

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	Renderer.SwapBuffer();
};

void UEditorEngine::Shutdown()
{
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	Renderer.Release();
};
