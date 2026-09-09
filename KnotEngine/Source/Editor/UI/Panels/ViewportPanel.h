#pragma once

class FInputRouter;
class FLevelEditorViewportClient;
class FViewport;
class IImGuiRenderBackend;

// ViewportPanel은 Level Editor의 Viewport를 ImGui Window로 감싸서 렌더링하고, InputRouter에 ViewportClient를 등록한다.
class FViewportPanel
{
public:
	FViewportPanel(FViewport& InViewport, FLevelEditorViewportClient& InViewportClient, IImGuiRenderBackend& InRenderBackend, FInputRouter& InInputRouter);

	void Draw(bool bVisible);

private:
	FViewport& Viewport;
	FLevelEditorViewportClient& ViewportClient;
	IImGuiRenderBackend& RenderBackend;
	FInputRouter& InputRouter;
};
