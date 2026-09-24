#pragma once

#include "Editor/Widget/ViewportOverlayWidget.h"
#include "Editor/Widget/ViewportWidget.h"
#include "Viewport/Level/LevelEditorViewportClient.h"

class FInputRouter;
class FRenderSystem;
class FViewportToolbar;
struct FEditorSelection;
struct ImVec2;

// ViewportPanel은 Level Editor의 Viewport를 ImGui Window로 감싸서 렌더링하고, InputRouter에 ViewportClient를 등록한다.
class FViewportPanel
{
public:
	FViewportPanel(
		FRenderSystem& InRenderSystem,
		FInputRouter& InInputRouter,
		const FViewportStatState& InStatState);
	~FViewportPanel();

	void Draw(bool bVisible, float DeltaTime, FViewportToolbar& Toolbar);
	void Release();
	FLevelEditorViewportClient& GetViewportClient() { return ViewportClient; }

private:
	void DrawContextMenu();
	void DrawBoxSelection(const ImVec2& ImageMinimum, const ImVec2& ImageMaximum) const;

	FViewportWidget ViewportWidget;
	FLevelEditorViewportClient ViewportClient;
	FViewportOverlayWidget ViewportOverlayWidget;
	FEditorSelection& Selection;

	FVector ContextMenuPlacementLocation = FVector::ZeroVector;
};
