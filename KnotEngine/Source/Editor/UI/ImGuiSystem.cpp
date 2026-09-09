#include "UI/ImGuiSystem.h"

#include "Core/Assert.h"
#include "Core/IO/Paths.h"
#include "Input/InputRouter.h"
#include "Render/ImGui/ImGuiRenderBackend.h"
#include "Viewport/LevelEditorViewportClient.h"
#include "Viewport/Viewport.h"
#include "World/World.h"

#include <filesystem>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_win32.h>
#include <string>
#include <system_error>

FImGuiSystem::FImGuiSystem(
	IImGuiRenderBackend& InRenderBackend,
	FInputRouter& InInputRouter,
	FViewport& InViewport,
	FLevelEditorViewportClient& InViewportClient)
	: RenderBackend(InRenderBackend), InputRouter(InInputRouter),
	  ViewportPanel(InViewport, InViewportClient, InRenderBackend, InInputRouter)
{
}

void FImGuiSystem::Startup(HWND WindowHandle)
{
	checkf(WindowHandle, "HWND 생성 실패.");
	panicf(ImGui::CreateContext(), "ImGui Context 생성 실패.");

	std::error_code FileSystemError;
	std::filesystem::create_directories(FPaths::SettingDir(), FileSystemError);
	panicf(!FileSystemError, "ImGui 설정 디렉터리 생성 실패. Error={}", FileSystemError.message());

	static const std::string ImGuiSettingsPath = FPaths::ToUtf8(FPaths::ImGuiSettingsPath());
	ImGuiIO& IO = ImGui::GetIO();
	IO.IniFilename = ImGuiSettingsPath.c_str();
	IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	IO.ConfigDpiScaleFonts = true;
	IO.ConfigDpiScaleViewports = true;

	panicf(ImGui_ImplWin32_Init(WindowHandle), "ImGui Win32 플랫폼 백엔드 초기화 실패.");

	RenderBackend.Startup(ImGui::GetCurrentContext());
	ConsolePanel.Startup();
}

void FImGuiSystem::BeginFrame()
{
	RenderBackend.BeginFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	const ImGuiIO& IO = ImGui::GetIO();
	InputRouter.SetImGuiCaptureState(IO.WantCaptureMouse, IO.WantCaptureKeyboard, IO.WantTextInput);
}

void FImGuiSystem::Draw(UWorld& World, float DeltaTime)
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

	DrawMenuBar();
	const ImGuiID DockspaceId = ImGui::GetID("KnotEditorDockspace");
	const bool bNeedsDefaultLayout = ImGui::DockBuilderGetNode(DockspaceId) == nullptr;
	ImGui::DockSpaceOverViewport(DockspaceId, ImGui::GetMainViewport(), ImGuiDockNodeFlags_None);
	if (bNeedsDefaultLayout)
	{
		BuildLayout(DockspaceId);
	}
	if (bShowHierarchy)
	{
		HierarchyPanel.Draw(World, Selection);
	}
	if (bShowInspector)
	{
		InspectorPanel.Draw(Selection);
	}
	ViewportPanel.Draw(bShowViewport);
	if (bShowConsole)
	{
		ConsolePanel.Draw();
	}

	const ImGuiIO& IO = ImGui::GetIO();
	InputRouter.SetImGuiCaptureState(IO.WantCaptureMouse, IO.WantCaptureKeyboard, IO.WantTextInput);
}

void FImGuiSystem::Render(FCommandListHandle CommandList)
{
	// Draw()에서 쌓은 UI 명령을 확정한 뒤, World 렌더링이 끝난 Viewport Texture를 포함한 ImGui Draw Data를 Back Buffer에 렌더링한다.
	ImGui::Render();
	RenderBackend.Render(CommandList, ImGui::GetDrawData());
}

void FImGuiSystem::Shutdown()
{
	ConsolePanel.Shutdown();
	RenderBackend.Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void FImGuiSystem::DrawMenuBar()
{
	if (!ImGui::BeginMainMenuBar())
	{
		return;
	}
	if (ImGui::BeginMenu("Window"))
	{
		ImGui::MenuItem("Hierarchy", nullptr, &bShowHierarchy);
		ImGui::MenuItem("Inspector", nullptr, &bShowInspector);
		ImGui::MenuItem("Viewport", nullptr, &bShowViewport);
		ImGui::MenuItem("Console", nullptr, &bShowConsole);
		ImGui::EndMenu();
	}
	ImGui::Separator();
	ImGui::Text("FPS %.1f | %.3f ms", DisplayedFramesPerSecond, DisplayedFrameTimeMs);
	ImGui::EndMainMenuBar();
}

void FImGuiSystem::BuildLayout(std::uint32_t DockspaceId)
{
	const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
	ImGui::DockBuilderRemoveNode(DockspaceId);
	ImGui::DockBuilderAddNode(DockspaceId, ImGuiDockNodeFlags_DockSpace);
	ImGui::DockBuilderSetNodePos(DockspaceId, MainViewport->WorkPos);
	ImGui::DockBuilderSetNodeSize(DockspaceId, MainViewport->WorkSize);

	ImGuiID CenterId = DockspaceId;
	ImGuiID LeftId = 0;
	ImGuiID RightId = 0;
	ImGuiID BottomId = 0;
	ImGui::DockBuilderSplitNode(CenterId, ImGuiDir_Left, 0.20f, &LeftId, &CenterId);
	ImGui::DockBuilderSplitNode(CenterId, ImGuiDir_Right, 0.25f, &RightId, &CenterId);
	ImGui::DockBuilderSplitNode(CenterId, ImGuiDir_Down, 0.25f, &BottomId, &CenterId);
	ImGui::DockBuilderDockWindow("Hierarchy", LeftId);
	ImGui::DockBuilderDockWindow("Inspector", RightId);
	ImGui::DockBuilderDockWindow("Console", BottomId);
	ImGui::DockBuilderDockWindow("Viewport", CenterId);
	ImGui::DockBuilderFinish(DockspaceId);
}
