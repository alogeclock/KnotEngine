#include "UI/EditorUISystem.h"

#include "Core/Assert.h"
#include "Core/IO/Paths.h"
#include "Input/InputRouter.h"
#include "UI/ImGui/ImGuiRenderBackend.h"

#include <filesystem>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <string>
#include <system_error>

FEditorUISystem::FEditorUISystem(IImGuiRenderBackend& InRenderBackend, FInputRouter& InInputRouter)
	: RenderBackend(InRenderBackend), InputRouter(InInputRouter)
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

	const ImGuiIO& IO = ImGui::GetIO();
	InputRouter.SetImGuiCaptureState(IO.WantCaptureMouse, IO.WantCaptureKeyboard, IO.WantTextInput);
}

void FEditorUISystem::Draw(float DeltaTime)
{
	// TO-DO: 프레임 통계는 별도 Overlay Panel로 분리하여 콘솔을 통해 출력할 수 있도록 한다.
	if (DeltaTime > 0.0f)
	{
		ElapsedTime += DeltaTime;
		++FrameCount;
	}

	if (ElapsedTime >= 0.5f)
	{
		DisplayedFramesPerSecond = static_cast<float>(FrameCount) / ElapsedTime;
		DisplayedFrameTimeMs = ElapsedTime / static_cast<float>(FrameCount) * 1000.0f;
		ElapsedTime = 0.0f;
		FrameCount = 0;
	}

	ImGui::Begin("Knot Engine Property Window");
	ImGui::Text("FPS: %.1f (%.3f ms)", DisplayedFramesPerSecond, DisplayedFrameTimeMs);
	ImGui::Separator();
	ImGui::ColorEdit4("Background Color", ClearColor);
	ImGui::End();

	const ImGuiIO& IO = ImGui::GetIO();
	InputRouter.SetImGuiCaptureState(IO.WantCaptureMouse, IO.WantCaptureKeyboard, IO.WantTextInput);
}

void FEditorUISystem::EndFrame(FCommandListHandle CommandList)
{
	ImGui::Render();
	RenderBackend.Render(CommandList, ImGui::GetDrawData());
}

void FEditorUISystem::Shutdown()
{
	RenderBackend.Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}
