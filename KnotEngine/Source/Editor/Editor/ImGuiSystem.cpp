#include "Editor/ImGuiSystem.h"

#include "Asset/AssetImportManager.h"
#include "Core/Assert.h"
#include "Platform/WindowsApplication.h"
#include "Core/IO/Paths.h"
#include "Input/InputRouter.h"
#include "Core/Profiling/CPUProfiler.h"
#include "Render/ImGui/ImGuiRenderBackend.h"
#include "Runtime/EditorEngine.h"
#include "Asset/Resource/resource.h"
#include "World/World.h"

#include <algorithm>
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
	FAssetRegistry& InAssetRegistry,
	FAssetImportManager& InAssetImportManager,
	IRenderDevice& InRenderDevice,
	IImGuiRenderBackend& InRenderBackend,
	FInputRouter& InInputRouter)
	: Application(InApplication), EditorEngine(InEditorEngine), AssetImportManager(InAssetImportManager),
	  RenderBackend(InRenderBackend), InputRouter(InInputRouter),
	  InspectorPanel(InAssetRegistry), ViewportPanel(InRenderDevice, InRenderBackend, InInputRouter, ViewportStatState, Selection), ConsolePanel(ViewportStatState),
	  ContentPanel(InAssetRegistry, InAssetImportManager, InRenderDevice, InRenderBackend)
{
	EditorEngine.RegisterViewportClient(ViewportPanel.GetViewportClient());
}

FImGuiSystem::~FImGuiSystem()
{
	InputRouter.UnregisterTarget(*this);
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
	InspectorPanel.SetBoldFont(*SemiBoldFont);

	panicf(ImGui_ImplWin32_Init(WindowHandle), "ImGui Win32 플랫폼 백엔드 초기화 실패.");

	RenderBackend.Startup(ImGui::GetCurrentContext());
	ConsolePanel.Startup();
	ContentPanel.Startup();
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
	KNOT_PROFILE_SCOPE("Tick", "FImGuiSystem::Draw");

	InputRouter.RegisterGlobalKeyTarget(*this);
	DrawMenuBar();
	DrawBottomToolbar();
	const ImGuiID DockspaceId = ImGui::GetID("KnotEditorDockspaceV2");
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
	ViewportPanel.Draw(bShowViewport, DeltaTime);
	DrawBottomPanelDockspace();
	if (bShowContent)
	{
		ContentPanel.Draw();
		if (bFocusContentRequested)
		{
			ImGui::SetWindowFocus("Content");
			bFocusContentRequested = false;
		}
	}
	if (bShowConsole)
	{
		ConsolePanel.Draw();
		if (bFocusConsoleRequested)
		{
			ImGui::SetWindowFocus("Console");
			bFocusConsoleRequested = false;
		}
	}
#if KNOT_CPU_PROFILER_ENABLED
	if (bShowProfile)
	{
		ProfilePanel.Draw(DeltaTime);
		if (bFocusProfileRequested)
		{
			ImGui::SetWindowFocus("Profile");
			bFocusProfileRequested = false;
		}
	}
#endif

	const ImGuiIO& IO = ImGui::GetIO();
	InputRouter.SetImGuiCaptureState(IO.WantCaptureMouse, IO.WantCaptureKeyboard, IO.WantTextInput);
}

// Snapshot에서 Router가 전달한 전역 단축키로 Bottom Panel을 토글한다.
FInputReply FImGuiSystem::OnInputEvent(const FInputEvent& Event)
{
	const FKeyInputEvent* KeyEvent = std::get_if<FKeyInputEvent>(&Event);
	if (!KeyEvent || !KeyEvent->bDown || KeyEvent->bRepeat)
	{
		return FInputReply::Unhandled();
	}

	if (KeyEvent->Key == EKeyboardKey::Tilde)
	{
		bShowConsole = !bShowConsole;
		bFocusConsoleRequested = bShowConsole;
		if (bShowConsole)
		{
			ConsolePanel.RequestCommandInputFocus();
		}
		return FInputReply::Handled();
	}
	if (KeyEvent->Key != EKeyboardKey::Space)
	{
		return FInputReply::Unhandled();
	}

	const bool bControlDown = HasModifierKey(KeyEvent->Modifiers, EModifierKeyMask::Control);
	const bool bShiftDown = HasModifierKey(KeyEvent->Modifiers, EModifierKeyMask::Shift);
	if (bControlDown && !bShiftDown)
	{
		bShowContent = !bShowContent;
		bFocusContentRequested = bShowContent;
		return FInputReply::Handled();
	}
#if KNOT_CPU_PROFILER_ENABLED
	if (bShiftDown && !bControlDown)
	{
		bShowProfile = !bShowProfile;
		bFocusProfileRequested = bShowProfile;
		return FInputReply::Handled();
	}
#endif
	return FInputReply::Unhandled();
}

// Main Viewport 하단에 비동기 작업 상태를 표시하고 Dockspace에서 Toolbar 영역을 제외한다.
void FImGuiSystem::DrawBottomToolbar()
{
	const float ToolbarHeight = ImGui::GetFrameHeight();
	const ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;
	const ImVec2 WindowPadding(0.0f, ImGui::GetStyle().WindowPadding.y);
	const ImVec2 MenuBarItemSpacing(0.0f, ImGui::GetStyle().ItemSpacing.y);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, WindowPadding);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, MenuBarItemSpacing);
	if (ImGui::BeginViewportSideBar("##BottomToolbar", ImGui::GetMainViewport(), ImGuiDir_Down, ToolbarHeight, WindowFlags))
	{
		const bool bMenuBarVisible = ImGui::BeginMenuBar();
		ImGui::PopStyleVar();
		if (bMenuBarVisible)
		{
			const auto TogglePanel = [](bool& bShowPanel, bool& bFocusRequested)
			{
				bShowPanel = !bShowPanel;
				bFocusRequested = bShowPanel;
			};
			const FAssetImportStatus Status = AssetImportManager.GetStatus();
			FString StatusText;
			float ImportStatusWidth = 0.0f;
			if (Status.bRunning)
			{
				const FString FileName = FPaths::ToUtf8(Status.SourceFilePath.filename().wstring());
				StatusText = "Importing " + FileName + "..." + "  |  " + std::to_string(static_cast<uint32>(Status.ElapsedSeconds)) + "s" +
					(Status.QueuedCount > 0 ? "  |  " + std::to_string(Status.QueuedCount) + " queued" : "");
				ImportStatusWidth = ImGui::GetFontSize() * 0.8f + ImGui::GetStyle().ItemSpacing.x + ImGui::CalcTextSize(StatusText.c_str()).x;
			}

			const auto DrawToolbarButton = [](const char* Label)
			{
				const ImVec4 ToolbarColor = ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg);
				ImGui::PushStyleColor(ImGuiCol_Button, ToolbarColor);
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
				ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
				const bool bClicked = ImGui::Button(Label, ImVec2(0.0f, ImGui::GetFrameHeight()));
				ImGui::PopStyleVar();
				ImGui::PopStyleColor(3);
				const ImVec2 Minimum = ImGui::GetItemRectMin();
				const ImVec2 Maximum = ImGui::GetItemRectMax();
				const ImU32 BorderColor = ImGui::GetColorU32(ImGuiCol_Border);
				ImDrawList* DrawList = ImGui::GetWindowDrawList();
				DrawList->AddLine(ImVec2(Minimum.x, Minimum.y), ImVec2(Minimum.x, Maximum.y), BorderColor);
				DrawList->AddLine(ImVec2(Maximum.x, Minimum.y), ImVec2(Maximum.x, Maximum.y), BorderColor);
				return bClicked;
			};

			if (DrawToolbarButton("Content"))
			{
				TogglePanel(bShowContent, bFocusContentRequested);
			}
			ImGui::SameLine(0.0f, 0.0f);
			if (DrawToolbarButton("Console"))
			{
				TogglePanel(bShowConsole, bFocusConsoleRequested);
			}

			float RightContentWidth = ImportStatusWidth;
#if KNOT_CPU_PROFILER_ENABLED
			const float ProfileButtonWidth = ImGui::CalcTextSize("Profile").x + ImGui::GetStyle().FramePadding.x * 2.0f;
			RightContentWidth += ProfileButtonWidth + (Status.bRunning ? ImGui::GetStyle().ItemSpacing.x : 0.0f);
#endif
			const float RightContentX = ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x - RightContentWidth;
			if (RightContentWidth > 0.0f)
			{
				ImGui::SameLine();
				ImGui::SetCursorPosX((std::max)(ImGui::GetCursorPosX(), RightContentX));
			}
			if (Status.bRunning)
			{
				const float SpinnerSize = ImGui::GetFontSize() * 0.4f;
				const ImVec2 Center(ImGui::GetCursorScreenPos().x + SpinnerSize, ImGui::GetCursorScreenPos().y + ImGui::GetFrameHeight() * 0.5f);
				ImDrawList* DrawList = ImGui::GetWindowDrawList();
				const float StartAngle = static_cast<float>(ImGui::GetTime() * 5.0);
				DrawList->PathArcTo(Center, SpinnerSize, StartAngle, StartAngle + 4.7f, 20);
				DrawList->PathStroke(ImGui::GetColorU32(ImGuiCol_CheckMark), 0, 2.8f);
				ImGui::Dummy(ImVec2(SpinnerSize * 2.0f, ImGui::GetFrameHeight()));
				ImGui::SameLine();
				ImGui::TextUnformatted(StatusText.c_str());
			}
#if KNOT_CPU_PROFILER_ENABLED
			if (Status.bRunning)
			{
				ImGui::SameLine();
			}
			if (DrawToolbarButton("Profile"))
			{
				TogglePanel(bShowProfile, bFocusProfileRequested);
			}
#endif
			ImGui::EndMenuBar();
		}
	}
	else
	{
		ImGui::PopStyleVar();
	}
	ImGui::End();
	ImGui::PopStyleVar();
}

// Content, Console, Profile 중 하나가 열리면 Main Viewport 하단 전체 폭에 Drawer Dockspace를 표시한다.
void FImGuiSystem::DrawBottomPanelDockspace()
{
	bool bShowBottomPanel = bShowContent || bShowConsole;
#if KNOT_CPU_PROFILER_ENABLED
	bShowBottomPanel = bShowBottomPanel || bShowProfile;
#endif
	if (!bShowBottomPanel)
	{
		return;
	}

	const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
	const float DrawerHeight = MainViewport->WorkSize.y * 0.4f;
	ImGui::SetNextWindowPos(ImVec2(MainViewport->WorkPos.x, MainViewport->WorkPos.y + MainViewport->WorkSize.y - DrawerHeight));
	ImGui::SetNextWindowSize(ImVec2(MainViewport->WorkSize.x, DrawerHeight));
	ImGui::SetNextWindowViewport(MainViewport->ID);
	constexpr ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
		| ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	if (ImGui::Begin("##BottomPanelDrawer", nullptr, WindowFlags))
	{
		const ImGuiID DockspaceId = ImGui::GetID("KnotEditorBottomPanelDockspace");
		constexpr ImGuiDockNodeFlags DockspaceFlags = ImGuiDockNodeFlags_NoDockingSplit | ImGuiDockNodeFlags_NoResize | ImGuiDockNodeFlags_NoUndocking;
		if (ImGui::DockBuilderGetNode(DockspaceId) == nullptr)
		{
			ImGui::DockBuilderAddNode(DockspaceId, ImGuiDockNodeFlags_DockSpace | DockspaceFlags);
			ImGui::DockBuilderSetNodeSize(DockspaceId, ImGui::GetContentRegionAvail());
			ImGui::DockBuilderDockWindow("Content", DockspaceId);
			ImGui::DockBuilderDockWindow("Console", DockspaceId);
#if KNOT_CPU_PROFILER_ENABLED
			ImGui::DockBuilderDockWindow("Profile", DockspaceId);
#endif
			ImGui::DockBuilderFinish(DockspaceId);
		}
		ImGui::DockSpace(DockspaceId, ImVec2(0.0f, 0.0f), DockspaceFlags);
	}
	ImGui::End();
	ImGui::PopStyleVar(2);
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
	ContentPanel.Shutdown();
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
	const ImVec2 WindowPadding(0.0f, ImGui::GetStyle().WindowPadding.y);
	const ImVec2 MenuBarItemSpacing(0.0f, ImGui::GetStyle().ItemSpacing.y);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, WindowPadding);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, MenuBarItemSpacing);
	ImGuiStyle& Style = ImGui::GetStyle();
	const float PreviousSafeAreaPaddingX = Style.DisplaySafeAreaPadding.x;
	Style.DisplaySafeAreaPadding.x = 0.0f;
	const bool bMenuBarVisible = ImGui::BeginMainMenuBar();
	Style.DisplaySafeAreaPadding.x = PreviousSafeAreaPaddingX;
	ImGui::PopStyleVar();
	if (!bMenuBarVisible)
	{
		ImGui::PopStyleVar();
		ImGui::PopFont();
		return;
	}
	ImDrawList* MenuBarDrawList = ImGui::GetWindowDrawList();
	const float MenuBarTop = ImGui::GetWindowPos().y;
	const float MenuBarBottom = MenuBarTop + ImGui::GetWindowHeight();
	const bool bWindowMenuOpen = ImGui::BeginMenu("Window");
	const ImVec2 WindowMenuMinimum = ImGui::GetItemRectMin();
	const ImVec2 WindowMenuMaximum = ImGui::GetItemRectMax();
	const ImU32 BorderColor = ImGui::GetColorU32(ImGuiCol_Border);
	MenuBarDrawList->AddLine(ImVec2(WindowMenuMinimum.x, MenuBarTop), ImVec2(WindowMenuMinimum.x, MenuBarBottom), BorderColor);
	MenuBarDrawList->AddLine(ImVec2(WindowMenuMaximum.x, MenuBarTop), ImVec2(WindowMenuMaximum.x, MenuBarBottom), BorderColor);
	if (bWindowMenuOpen)
	{
		ImGui::MenuItem("Hierarchy", nullptr, &bShowHierarchy);
		ImGui::MenuItem("Inspector", nullptr, &bShowInspector);
		ImGui::MenuItem("Viewport", nullptr, &bShowViewport);
		ImGui::MenuItem("Console", nullptr, &bShowConsole);
		ImGui::MenuItem("Content", nullptr, &bShowContent);
#if KNOT_CPU_PROFILER_ENABLED
		ImGui::MenuItem("Profile", nullptr, &bShowProfile);
#endif
		ImGui::EndMenu();
	}
	ImGui::EndMainMenuBar();
	ImGui::PopStyleVar();
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
	ImGui::DockBuilderSplitNode(CenterId, ImGuiDir_Left, 0.20f, &LeftId, &CenterId);
	ImGui::DockBuilderSplitNode(CenterId, ImGuiDir_Right, 0.25f, &RightId, &CenterId);
	ImGui::DockBuilderDockWindow("Hierarchy", LeftId);
	ImGui::DockBuilderDockWindow("Inspector", RightId);
	ImGui::DockBuilderDockWindow("Viewport", CenterId);
	ImGui::DockBuilderFinish(DockspaceId);
}
