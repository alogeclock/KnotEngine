#pragma once

#include "Render/RHI/RenderTypes.h"
#include "Editor/EditorSelection.h"
#include "Input/InputRouter.h"
#include "Editor/Panels/ConsolePanel.h"
#include "Editor/Panels/ContentPanel.h"
#include "Editor/Panels/HierarchyPanel.h"
#include "Editor/Panels/InspectorPanel.h"
#include "Editor/Panels/ProfilePanel.h"
#include "Editor/Panels/ViewportPanel.h"

#include <Windows.h>
#include <cstdint>
#include <filesystem>
#include <optional>

class IImGuiRenderBackend;
class IRenderDevice;
class FAssetImportManager;
class FAssetRegistry;
struct ImFont;
class UEditorEngine;
class FWindowsApplication;

// Editor의 ImGui Context와 Panel 수명주기 및 Frame 렌더링을 관리한다.
class FImGuiSystem : public IInputTarget
{
public:
	FImGuiSystem(
		FWindowsApplication& InApplication,
		UEditorEngine& InEditorEngine,
		FAssetRegistry& InAssetRegistry,
		FAssetImportManager& InAssetImportManager,
		IRenderDevice& InRenderDevice,
		IImGuiRenderBackend& InRenderBackend,
		FInputRouter& InInputRouter,
		FEditorSelection& InSelection);
	~FImGuiSystem();

	void Startup();
	void BeginFrame();
	void Draw(float DeltaTime);
	void EndFrame();
	void Render(FCommandListHandle CommandList);
	void Shutdown();

private:
	FInputReply OnInputEvent(const FInputEvent& Event) override;

	void ProcessLevelDialogs();
	void LoadLevel();
	void SaveLevel(bool bSaveAs);
	std::optional<std::filesystem::path> OpenLevelDialog(bool bSave) const;

	FWindowsApplication& Application;
	UEditorEngine& EditorEngine;
	FAssetImportManager& AssetImportManager;
	IImGuiRenderBackend& RenderBackend;
	FInputRouter& InputRouter;
	FEditorSelection& Selection;

	ImFont* MediumFont = nullptr;
	ImFont* SemiBoldFont = nullptr;

	FViewportStatState ViewportStatState;

	FHierarchyPanel HierarchyPanel;
	FInspectorPanel InspectorPanel;
	FViewportPanel ViewportPanel;
	FConsolePanel ConsolePanel;
	FContentPanel ContentPanel;
#if KNOT_CPU_PROFILER_ENABLED
	FProfilePanel ProfilePanel;
#endif

	bool bStarted = false;

	bool bShowHierarchy = true;
	bool bShowInspector = true;
	bool bShowViewport = true;
	bool bShowConsole = false;
	bool bShowContent = false;

	bool bFocusConsoleRequested = false;
	bool bFocusContentRequested = false;
	bool bLoadLevelDialogRequested = false;
	bool bSaveLevelDialogRequested = false;

#if KNOT_CPU_PROFILER_ENABLED
	bool bShowProfile = false;
	bool bFocusProfileRequested = false;
#endif

	void DrawMenuBar();
	void DrawBottomToolbar();
	void DrawBottomPanelDockspace();

	void BuildLayout(std::uint32_t DockspaceId);
};
