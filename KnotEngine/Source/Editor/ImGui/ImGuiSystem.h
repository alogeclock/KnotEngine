#pragma once

#include "Render/RHI/RenderTypes.h"
#include "ImGui/EditorSelection.h"
#include "ImGui/Panels/ConsolePanel.h"
#include "ImGui/Panels/HierarchyPanel.h"
#include "ImGui/Panels/InspectorPanel.h"
#include "ImGui/Panels/ViewportPanel.h"

#include <Windows.h>
#include <cstdint>

class IImGuiRenderBackend;
class IRenderDevice;
class FInputRouter;
class UEditorEngine;

class FImGuiSystem
{
public:
	FImGuiSystem(UEditorEngine& InEditorEngine, IRenderDevice& InRenderDevice, IImGuiRenderBackend& InRenderBackend, FInputRouter& InInputRouter);
	~FImGuiSystem();

	void Startup(HWND WindowHandle);
	void BeginFrame();
	void Draw(float DeltaTime);
	void EndFrame();
	void Render(FCommandListHandle CommandList);
	void Shutdown();

private:
	UEditorEngine& EditorEngine;
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
	void BuildLayout(std::uint32_t DockspaceId);
};
