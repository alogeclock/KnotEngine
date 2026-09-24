#pragma once

#include "Render/RHI/RenderTypes.h"
#include "Render/ImGui/ImGuiDrawDataCopy.h"
#include "Editor/Context/EditorSelection.h"
#include "Input/InputRouter.h"
#include "Editor/Panel/ConsolePanel.h"
#include "Editor/Panel/ContentPanel.h"
#include "Editor/Panel/HierarchyPanel.h"
#include "Editor/Panel/InspectorPanel.h"
#include "Editor/Panel/ProfilePanel.h"
#include "Editor/Panel/SettingsPanel.h"
#include "Editor/Panel/ViewportPanel.h"
#include "Editor/Toolbar/ViewportToolbar.h"

#include <Windows.h>
#include <cstdint>
#include <filesystem>
#include <optional>

class FRenderSystem;
class FAssetEditor;
struct ImFont;
struct ImDrawData;
class FWindowsApplication;

// Editor의 ImGui Context와 Panel 수명주기 및 Frame 렌더링을 관리한다.
class FImGuiSystem : public IInputTarget
{
public:
	explicit FImGuiSystem(FWindowsApplication& InApplication);
	~FImGuiSystem();

	void Startup();
	void BeginFrame();
	void Draw(float DeltaTime);
	void EndFrame();
	void Shutdown();

	FImGuiDrawDataCopy Consume();

private:
	FInputReply OnInputEvent(const FInputEvent& Event) override;

	void ProcessLevelDialogs();
	void LoadLevel();
	void SaveLevel(bool bSaveAs);
	void DuplicateSelection();
	std::optional<std::filesystem::path> OpenLevelDialog(bool bSave) const;

	void UpdateCursor();

	void OpenAssetEditor(const FAssetId& AssetId);
	void DrawAssetEditors(float DeltaTime);

	void DrawMenuBar();
	void DrawBottomToolbar();
	void DrawBottomPanelDockspace();

	void BuildLayout(std::uint32_t DockspaceId);

	FWindowsApplication& Application;

	TArray<std::unique_ptr<FAssetEditor>> AssetEditors;

	ImFont* MediumFont = nullptr;
	ImFont* SemiBoldFont = nullptr;
	FImGuiDrawDataCopy DrawData;
	std::optional<FVector2> ViewportCursorOrigin;

	FViewportStatState ViewportStatState;
	FViewportToolbar ViewportToolbar;

	FHierarchyPanel HierarchyPanel;
	FInspectorPanel InspectorPanel;
	FViewportPanel ViewportPanel;
	FConsolePanel ConsolePanel;
	FContentPanel ContentPanel;
	FSettingsPanel SettingsPanel;
#if KNOT_CPU_PROFILER_ENABLED
	FProfilePanel ProfilePanel;
#endif

	bool bStarted = false;

	bool bShowHierarchy = true;
	bool bShowInspector = true;
	bool bShowViewport = true;
	bool bShowConsole = false;
	bool bShowContent = false;

	bool bFocusContentRequested = false;
	bool bLoadLevelDialogRequested = false;
	bool bSaveLevelDialogRequested = false;
	bool bViewportCursorLocked = false;

#if KNOT_CPU_PROFILER_ENABLED
	bool bShowProfile = false;
	bool bFocusProfileRequested = false;
#endif

};
