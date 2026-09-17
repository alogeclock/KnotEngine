#pragma once

#include "Render/RHI/RenderTypes.h"
#include "Editor/EditorSelection.h"
#include "Editor/Panels/ConsolePanel.h"
#include "Editor/Panels/ContentPanel.h"
#include "Editor/Panels/HierarchyPanel.h"
#include "Editor/Panels/InspectorPanel.h"
#include "Editor/Panels/ProfilePanel.h"
#include "Editor/Panels/ViewportPanel.h"

#include <Windows.h>
#include <cstdint>

class IImGuiRenderBackend;
class IRenderDevice;
class FAssetRegistry;
class FInputRouter;
struct ImFont;
class UEditorEngine;
class FWindowsApplication;

class FImGuiSystem
{
public:
	FImGuiSystem(
		FWindowsApplication& InApplication,
		UEditorEngine& InEditorEngine,
		FAssetRegistry& InAssetRegistry,
		IRenderDevice& InRenderDevice,
		IImGuiRenderBackend& InRenderBackend,
		FInputRouter& InInputRouter);
	~FImGuiSystem();

	void Startup();
	void BeginFrame();
	void Draw(float DeltaTime);
	void EndFrame();
	void Render(FCommandListHandle CommandList);
	void Shutdown();

private:
	FWindowsApplication& Application;
	UEditorEngine& EditorEngine;
	IImGuiRenderBackend& RenderBackend;
	FInputRouter& InputRouter;
	FEditorSelection Selection;

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
	bool bShowConsole = true;
	bool bShowContent = true;
#if KNOT_CPU_PROFILER_ENABLED
	bool bShowProfile = true;
#endif

	void DrawMenuBar();
	void BuildLayout(std::uint32_t DockspaceId);
};
