#pragma once

#include "Viewport/Level/LevelEditorViewportClient.h"
#include "Viewport/Viewport.h"

class FInputRouter;
class IImGuiRenderBackend;
class IRenderDevice;

// ViewportPanel은 Level Editor의 Viewport를 ImGui Window로 감싸서 렌더링하고, InputRouter에 ViewportClient를 등록한다.
class FViewportPanel
{
public:
	FViewportPanel(IRenderDevice& InRenderDevice, IImGuiRenderBackend& InRenderBackend, FInputRouter& InInputRouter);
	~FViewportPanel();

	void Draw(bool bVisible);
	void Release();
	FLevelEditorViewportClient& GetViewportClient() { return ViewportClient; }

private:
	void DrawToolbar();
	void DrawViewport();

	FViewport Viewport;
	FLevelEditorViewportClient ViewportClient;
	IImGuiRenderBackend& RenderBackend;
	FInputRouter& InputRouter;
};
