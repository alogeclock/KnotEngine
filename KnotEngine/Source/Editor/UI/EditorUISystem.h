#pragma once

#include "Render/RHI/RenderTypes.h"
#include "UI/EditorSelection.h"
#include "UI/Panels/ConsolePanel.h"
#include "UI/Panels/HierarchyPanel.h"
#include "UI/Panels/InspectorPanel.h"
#include "UI/Panels/ViewportPanel.h"

#include <Windows.h>
#include <cstdint>

class IImGuiRenderBackend;
class FInputRouter;
class FLevelEditorViewportClient;
class FViewport;
class UWorld;

class FEditorUISystem
{
public:
	FEditorUISystem(IImGuiRenderBackend& InRenderBackend, FInputRouter& InInputRouter, FViewport& InViewport, FLevelEditorViewportClient& InViewportClient);

	void Startup(HWND WindowHandle);
	void BeginFrame();
	void Draw(UWorld& World, float DeltaTime);
	void Render(FCommandListHandle CommandList);
	void Shutdown();
	bool IsViewportVisible() const { return bShowViewport; }

private:
	IImGuiRenderBackend& RenderBackend;
	FInputRouter& InputRouter;
	FEditorSelection Selection;

	FHierarchyPanel HierarchyPanel;
	FInspectorPanel InspectorPanel;
	FViewportPanel ViewportPanel;
	FConsolePanel ConsolePanel;
	
	bool bShowHierarchy = true;
	bool bShowInspector = true;
	bool bShowViewport = true;
	bool bShowConsole = true;
	
	// TO-DO: 프레임 통계는 별도 Overlay Panel로 분리하여 콘솔을 통해 출력할 수 있도록 한다.
	float ElapsedTime = 0.0f;
	std::uint32_t FrameCount = 0;

	float DisplayedFramesPerSecond = 0.0f;
	float DisplayedFrameTimeMs = 0.0f;

	void DrawMenuBar();
	void BuildDefaultDockLayout(std::uint32_t DockspaceId);
};
