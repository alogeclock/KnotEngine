#pragma once

#include "Viewport/EditorViewportClient.h"
#include "TransformGizmo.h"
#include "Render/Scene/SceneBVH.h"
#include <deque>
#include <optional>

class FRenderer;
class UNode;
struct FEditorSelection;

class FLevelEditorViewportClient : public FEditorViewportClient
{
public:
	explicit FLevelEditorViewportClient(FViewport& InViewport);

	void Tick(float DeltaTime) override;
	FInputReply OnInputEvent(const FInputEvent& Event) override;

	void OnKeyboardFocusLost() override;
	void OnMouseCaptureLost() override;
	UWorld* GetWorld() const override;

	FSceneView BuildSceneView() override;
	bool IsGizmoLocalSpace() const { return TransformGizmo.IsLocalSpace(); }
	void SetGizmoLocalSpace(bool bLocalSpace) { TransformGizmo.SetLocalSpace(bLocalSpace); }
	bool ConsumeContextMenuRequest(FVector& OutPlacementLocation);
	bool GetBoxSelection(FVector2& OutStart, FVector2& OutEnd, bool& bOutAdditive) const;

private:
	void RequestSelection(const FVector2& InputPosition, EModifierKeyMask Modifiers);
	void ApplySelectionResult();
	void ApplyBoxSelection();

	void RequestContextMenu(const FVector2& InputPosition);
	void ApplyPlacementResult();
	FVector FindPlacementLocation(const FRay& Ray);

	void FocusSelectedNode();
	void UpdateFocusAnimation(float DeltaTime);

	FEditorSelection& Selection;

	struct FPendingRayQuery
	{
		std::shared_ptr<FSceneRayQuery> Query;
		uint32 WorldUUID = 0;
		uint64 Generation = 0;
		EModifierKeyMask Modifiers = EModifierKeyMask::None;
	};
	std::deque<FPendingRayQuery> PendingRayQueries; // GT가 아직 적용하지 않은 Ray 요청을 보관한다. 다른 선택 동작이 없었다면 제출 순서대로 적용한다.

	struct FPendingPlacementQuery
	{
		std::shared_ptr<FSceneRayQuery> Query;
		uint32 WorldUUID = 0;
		FVector FallbackPosition = FVector::ZeroVector;
	};
	std::optional<FPendingPlacementQuery> PendingPlacementQuery; // 우클릭 배치 위치를 구하는 최신 요청 하나만 보관한다.

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
