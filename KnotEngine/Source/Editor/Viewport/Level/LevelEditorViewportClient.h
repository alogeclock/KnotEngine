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

private:
	UNode* Raycast(const FVector2& InputPosition);

	void FocusSelectedNode();
	void UpdateFocusAnimation(float DeltaTime);

	FEditorSelection& Selection;
	FTransformGizmo TransformGizmo;

	static constexpr float FocusAnimationDuration = 0.25f;
	FVector FocusStartLocation = FVector::ZeroVector;
	FVector FocusTargetLocation = FVector::ZeroVector;
	float FocusStartOrthoZoom = 0.0f;
	float FocusTargetOrthoZoom = 0.0f;
	float FocusElapsedTime = 0.0f;
	uint32 FocusTargetUUID = 0;
	FEditorViewportCameraTransform LastFocusTransform; // 외부 카메라 조작을 감지하기 위한 직전 적용 값.
	bool bFocusAnimating = false;
};
