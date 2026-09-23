#pragma once

#include "Viewport/EditorViewportClient.h"
#include "TransformGizmo.h"

class FRenderer;
class UNode;
struct FEditorSelection;

class FLevelEditorViewportClient : public FEditorViewportClient
{
public:
	FLevelEditorViewportClient(FViewport& InViewport, FEditorSelection& InSelection);

	void Tick(float DeltaTime) override;
	FInputReply OnInputEvent(const FInputEvent& Event) override;

	void OnKeyboardFocusLost() override;
	void OnMouseCaptureLost() override;

	FSceneView BuildSceneView() override;
	bool IsGizmoLocalSpace() const { return TransformGizmo.IsLocalSpace(); }
	void SetGizmoLocalSpace(bool bLocalSpace) { TransformGizmo.SetLocalSpace(bLocalSpace); }
	bool ConsumeContextMenuRequest(FVector& OutPlacementLocation);
	bool GetBoxSelection(FVector2& OutStart, FVector2& OutEnd, bool& bOutAdditive) const;

private:
	UNode* Raycast(const FVector2& InputPosition, FVector* OutHitPosition = nullptr);
	void ApplyBoxSelection();
	FVector FindPlacementLocation(const FVector2& InputPosition);
	void RequestContextMenu(const FVector2& InputPosition);

	void FocusSelectedNode();
	void UpdateFocusAnimation(float DeltaTime);

	FEditorSelection& Selection;
	FTransformGizmo TransformGizmo;

	static constexpr float FocusAnimationDuration = 0.25f;
	static constexpr float ContextMenuDragThresholdSquared = 16.0f;
	static constexpr float BoxSelectionDragThresholdSquared = 16.0f;

	FVector FocusStartLocation = FVector::ZeroVector;
	FVector FocusTargetLocation = FVector::ZeroVector;
	float FocusStartOrthoZoom = 0.0f;
	float FocusTargetOrthoZoom = 0.0f;
	float FocusElapsedTime = 0.0f;
	uint32 FocusTargetUUID = 0;
	FEditorViewportCameraTransform LastFocusTransform; // 외부 카메라 조작을 감지하기 위한 직전 적용 값.

	FVector2 RightClickStartPosition = FVector2::ZeroVector;
	FVector ContextMenuPlacementLocation = FVector::ZeroVector;
	float RightClickTravelSquared = 0.0f; // 우클릭 이동 거리가 짧으면 Context Menu 요청으로 판정.

	FVector2 BoxSelectionStart = FVector2::ZeroVector;
	FVector2 BoxSelectionEnd = FVector2::ZeroVector;
	EModifierKeyMask BoxSelectionModifiers = EModifierKeyMask::None;
	bool bTrackingBoxSelection = false; // 좌클릭 이동 거리가 짧으면 Node 단일 선택 요청으로 판정.
	bool bBoxSelecting = false;

	bool bTrackingRightClick = false;
	bool bContextMenuRequested = false;
	bool bFocusAnimating = false;
};
