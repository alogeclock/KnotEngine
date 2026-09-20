#pragma once

#include "Editor/Overlays/ViewportStatOverlay.h"
#include "Viewport/Level/LevelEditorViewportClient.h"
#include "Viewport/Viewport.h"

class FInputRouter;
class FRenderSystem;
struct FEditorSelection;

// ViewportPanel은 Level Editor의 Viewport를 ImGui Window로 감싸서 렌더링하고, InputRouter에 ViewportClient를 등록한다.
class FViewportPanel
{
public:
	FViewportPanel(
		FRenderSystem& InRenderSystem,
		FInputRouter& InInputRouter,
		const FViewportStatState& InStatState,
		FEditorSelection& InSelection);
	~FViewportPanel();

	void Draw(bool bVisible, float DeltaTime);
	void Release();
	FLevelEditorViewportClient& GetViewportClient() { return ViewportClient; }

private:
	void DrawToolbar();
	void DrawViewport();

	FViewport Viewport;
	FLevelEditorViewportClient ViewportClient;
	FViewportStatOverlay StatOverlay;
	
	FRenderSystem& RenderSystem;
	FInputRouter& InputRouter;
};
