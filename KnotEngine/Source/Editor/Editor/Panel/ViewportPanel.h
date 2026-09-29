#pragma once

#include "Editor/Widget/LevelViewportLayout.h"
#include "Editor/Widget/ViewportOverlayWidget.h"
#include "Editor/Widget/ViewportWidget.h"
#include "Viewport/Level/LevelEditorViewportClient.h"

#include <memory>

class FInputRouter;
class FRenderSystem;
class FViewportToolbar;
struct FEditorSelection;
struct ImVec2;

// ViewportPanel은 Level Editor의 Viewport를 ImGui Window로 감싸서 렌더링하고, InputRouter에 ViewportClient를 등록한다.
class FViewportPanel
{
public:
	FViewportPanel(FRenderSystem& InRenderSystem, FInputRouter& InInputRouter, const FViewportStatState& InStatState);
	~FViewportPanel();

	void Draw(bool bVisible, float DeltaTime, FViewportToolbar& Toolbar);
	void Release();
	TStaticArray<FLevelEditorViewportClient*, 4> GetViewportClients();

private:
	struct FLevelViewportSlot
	{
		FLevelViewportSlot(FRenderSystem& RenderSystem, FInputRouter& InputRouter) : Widget(RenderSystem, InputRouter), Client(Widget.GetViewport()) {}
		FViewportWidget Widget;
		FLevelEditorViewportClient Client;
	};

	void UpdateActiveViewport();
	void DrawContextMenu();
	void DrawViewportSlot(SIZE_T SlotIndex, const FLevelViewportPaneRect& PaneRect, FViewportToolbar& Toolbar);
	void DrawBoxSelection(const FLevelEditorViewportClient& ViewportClient, const ImVec2& ImageMinimum, const ImVec2& ImageMaximum) const;

	FInputRouter& InputRouter;
	TStaticArray<std::unique_ptr<FLevelViewportSlot>, 4> ViewportSlots;
	FLevelViewportLayout ViewportLayout;
	FViewportOverlayWidget ViewportOverlayWidget;
	FEditorSelection& Selection;

	FVector ContextMenuPlacementLocation = FVector::ZeroVector;
	TStaticArray<SIZE_T, 4> VisibleViewportIndices = {};

	SIZE_T ActiveViewportIndex = 0;
	SIZE_T ContextMenuViewportIndex = 0;
	uint32 VisibleViewportCount = 1;
};
