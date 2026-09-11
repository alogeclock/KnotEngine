#include "ImGui/ImGuiSystem.h"

#include "Core/Assert.h"
#include "Platform/WindowsApplication.h"
#include "Core/IO/Paths.h"
#include "Input/InputRouter.h"
#include "Render/ImGui/ImGuiRenderBackend.h"
#include "Runtime/EditorEngine.h"
#include "Source/Resource/resource.h"
#include "World/World.h"

#include <filesystem>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_win32.h>
#include <limits>
#include <span>
#include <string>
#include <system_error>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND WindowHandle, UINT Message, WPARAM WParam, LPARAM LParam);

FImGuiSystem::FImGuiSystem(
	FWindowsApplication& InApplication,
	UEditorEngine& InEditorEngine,
	IRenderDevice& InRenderDevice,
	IImGuiRenderBackend& InRenderBackend,
	FInputRouter& InInputRouter)
	: Application(InApplication), EditorEngine(InEditorEngine), RenderBackend(InRenderBackend), InputRouter(InInputRouter),
	  ViewportPanel(InRenderDevice, InRenderBackend, InInputRouter)
{
	EditorEngine.RegisterViewportClient(ViewportPanel.GetViewportClient());
}

FImGuiSystem::~FImGuiSystem()
{
	EditorEngine.UnregisterViewportClient(ViewportPanel.GetViewportClient());
}

void FImGuiSystem::Startup()
{
	check(!bStarted);
	const HWND WindowHandle = Application.GetWindow().GetHwnd();
	checkf(WindowHandle, "HWND 생성 실패.");
	panicf(ImGui::CreateContext(), "ImGui Context 생성 실패.");

	std::error_code FileSystemError;
	std::filesystem::create_directories(FPaths::ConfigDir(), FileSystemError);
	panicf(!FileSystemError, "ImGui 설정 디렉터리 생성 실패. Error={}", FileSystemError.message());

	static const std::string ImGuiSettingsPath = FPaths::ToUtf8(FPaths::ImGuiSettingsPath());
	ImGuiIO& IO = ImGui::GetIO();
	IO.IniFilename = ImGuiSettingsPath.c_str();
	IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	IO.ConfigDpiScaleFonts = true;
	IO.ConfigDpiScaleViewports = true;

	static const auto LoadResourceBytes = [](uint32 ResourceId)
	{
		HMODULE Module = GetModuleHandleW(nullptr);
		HRSRC ResourceInfo = FindResourceW(Module, MAKEINTRESOURCEW(ResourceId), RT_RCDATA);
		panicf(ResourceInfo, "내장 리소스를 찾지 못했습니다. ResourceId={}", ResourceId);
		HGLOBAL ResourceData = LoadResource(Module, ResourceInfo);
		panicf(ResourceData, "내장 리소스를 불러오지 못했습니다. ResourceId={}", ResourceId);
		const DWORD ResourceSize = SizeofResource(Module, ResourceInfo);
		const auto* Bytes = static_cast<const uint8*>(LockResource(ResourceData));
		panicf(Bytes && ResourceSize > 0, "내장 리소스 데이터가 비어 있습니다. ResourceId={}", ResourceId);
		return std::span<const uint8>(Bytes, ResourceSize);
	};
	const std::span<const uint8> MediumFontBytes = LoadResourceBytes(IDR_PRETENDARD_MEDIUM);
	const std::span<const uint8> SemiBoldFontBytes = LoadResourceBytes(IDR_PRETENDARD_SEMIBOLD);
	panicf(MediumFontBytes.size() <= static_cast<size_t>((std::numeric_limits<int>::max)()) &&
		SemiBoldFontBytes.size() <= static_cast<size_t>((std::numeric_limits<int>::max)()), "내장 폰트 데이터가 너무 큽니다.");

	ImFontConfig FontConfig;
	FontConfig.FontDataOwnedByAtlas = false;
	const ImWchar* GlyphRanges = IO.Fonts->GetGlyphRangesKorean();
	MediumFont = IO.Fonts->AddFontFromMemoryTTF(const_cast<uint8*>(MediumFontBytes.data()), static_cast<int>(MediumFontBytes.size()), 16.0f, &FontConfig, GlyphRanges);
	SemiBoldFont = IO.Fonts->AddFontFromMemoryTTF(const_cast<uint8*>(SemiBoldFontBytes.data()), static_cast<int>(SemiBoldFontBytes.size()), 16.0f, &FontConfig, GlyphRanges);
	panicf(MediumFont && SemiBoldFont, "Pretendard 폰트를 ImGui Font Atlas에 등록하지 못했습니다.");
	IO.FontDefault = MediumFont;

	panicf(ImGui_ImplWin32_Init(WindowHandle), "ImGui Win32 플랫폼 백엔드 초기화 실패.");

	RenderBackend.Startup(ImGui::GetCurrentContext());
	ConsolePanel.Startup();
	bStarted = true;
	Application.SetMessageHandler(ImGui_ImplWin32_WndProcHandler);
}

void FImGuiSystem::BeginFrame()
{
	RenderBackend.BeginFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	const ImGuiIO& IO = ImGui::GetIO();
	InputRouter.SetImGuiCaptureState(IO.WantCaptureMouse, IO.WantCaptureKeyboard, IO.WantTextInput);
}

void FImGuiSystem::Draw(float DeltaTime)
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
		if (UWorld* World = EditorEngine.GetWorld())
		{
			HierarchyPanel.Draw(*World, Selection);
		}
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

void FImGuiSystem::EndFrame()
{
	ImGui::Render();
}

void FImGuiSystem::Render(FCommandListHandle CommandList)
{
	// World 렌더링이 끝난 Viewport Texture를 포함한 ImGui Draw Data를 Back Buffer에 렌더링한다.
	RenderBackend.Render(CommandList, ImGui::GetDrawData());
}

void FImGuiSystem::Shutdown()
{
	check(bStarted);
	Application.SetMessageHandler(nullptr);
	bStarted = false;
	ConsolePanel.Shutdown();
	ViewportPanel.Release();
	RenderBackend.Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	MediumFont = nullptr;
	SemiBoldFont = nullptr;
}

void FImGuiSystem::DrawMenuBar()
{
	check(SemiBoldFont);
	ImGui::PushFont(SemiBoldFont);
	if (!ImGui::BeginMainMenuBar())
	{
		ImGui::PopFont();
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
	ImGui::PopFont();
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
