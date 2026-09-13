#pragma once

#include "Render/RHI/RenderTypes.h"
#include "ImGui/EditorSelection.h"
#include "ImGui/Panels/ConsolePanel.h"
#include "ImGui/Panels/HierarchyPanel.h"
#include "ImGui/Panels/InspectorPanel.h"
#include "ImGui/Panels/ProfilePanel.h"
#include "ImGui/Panels/ViewportPanel.h"

#include <Windows.h>
#include <cstdint>

class IImGuiRenderBackend;
class IRenderDevice;
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
	bool bStarted = false;
	UEditorEngine& EditorEngine;
	IImGuiRenderBackend& RenderBackend;
	FInputRouter& InputRouter;
	FEditorSelection Selection;
	ImFont* MediumFont = nullptr;
	ImFont* SemiBoldFont = nullptr;

	FHierarchyPanel HierarchyPanel;
	FInspectorPanel InspectorPanel;
	FViewportPanel ViewportPanel;
	FConsolePanel ConsolePanel;
#if KNOT_CPU_PROFILER_ENABLED
	FProfilePanel ProfilePanel;
#endif

	bool bShowHierarchy = true;
	bool bShowInspector = true;
	bool bShowViewport = true;
	bool bShowConsole = true;
#if KNOT_CPU_PROFILER_ENABLED
	bool bShowProfile = true;
#endif

	void DrawMenuBar();
	void BuildLayout(std::uint32_t DockspaceId);
};
