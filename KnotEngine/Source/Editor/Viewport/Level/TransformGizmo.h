#pragma once

#include "Core/Geometry/Transform.h"
#include "Core/Input/InputEvents.h"
#include "Core/Geometry/Ray.h"
#include "Core/Math/Matrix.h"
#include "Core/Math/Vector2.h"
#include "Render/Scene/SceneView.h"

class FInputReply;
class UNode;
struct FEditorSelection;

enum class ETransformGizmoAxis : int8
{
	None = -1,
	X,
	Y,
	Z,
	View,
	Trackball,
	Center,
};

// 선택 Node의 Transform을 시작 상태 기준으로 조작하고 View별 Render 값만 생성한다.
class FTransformGizmo final
{
public:
	FInputReply OnInputEvent(const FInputEvent& Event, const FEditorSelection& Selection, const FSceneView& View, const FVector2& PixelPosition);
	void OnMouseCaptureLost();
	void OnKeyboardFocusLost();

	void UpdateSelection(const FEditorSelection& Selection);
	void BuildGizmoView(const FEditorSelection& Selection, const FSceneView& View, FGizmoView& OutData) const;

	bool IsDragging() const { return bDragging; }
	bool IsLocalSpace() const { return bLocalSpace; }
	void SetLocalSpace(bool bInLocalSpace);

private:
	struct FDragTarget
	{
		FTransform StartRelativeTransform;
		FMatrix StartWorldMatrix = FMatrix::Identity;
		FMatrix StartParentWorldInverse = FMatrix::Identity;
		uint32 NodeUUID = 0;
	};

	FVector GetAxisVector(ETransformGizmoAxis Axis) const;
	static FVector GetViewRotationAxis(const FSceneView& View, const FVector& Origin);
	static FVector MapTrackballVector(const FSceneView& View, const FVector& Origin, const FVector2& PixelPosition);

	static float GetUnitsPerPixel(const FSceneView& View, const FVector& WorldPosition);
	static bool WorldToScreen(const FSceneView& View, const FVector& WorldPosition, FVector2& OutPixelPosition);
	static float DistanceToSegmentSquared(const FVector2& Point, const FVector2& Start, const FVector2& End);
	static bool IntersectAxis(const FRay& Ray, const FVector& Origin, const FVector& Axis, float& OutAxisParameter);
	static bool IntersectPlane(const FRay& Ray, const FVector& PlaneOrigin, const FVector& PlaneNormal, FVector& OutPosition);

	ETransformGizmoAxis HitTest(const FSceneView& View, const FVector2& PixelPosition, const FVector& Origin) const;

	void ApplyTranslation(const FSceneView& View, const FVector2& PixelPosition);
	void ApplyRotation(const FSceneView& View, const FVector2& PixelPosition);
	void ApplyScale(const FSceneView& View, const FVector2& PixelPosition);
	bool ApplyWorldDelta(const FMatrix& WorldDelta);
	void RestoreDragTargets();

	bool BeginDrag(const FEditorSelection& Selection, ETransformGizmoAxis Axis, const FSceneView& View, const FVector2& PixelPosition);
	void UpdateDrag(const FEditorSelection& Selection, const FSceneView& View, const FVector2& PixelPosition);
	void CancelDrag();
	void EndDrag();

	UNode* ResolveNode(uint32 NodeUUID) const;
	bool IsDragSelectionValid(const FEditorSelection& Selection) const;

	static constexpr float GizmoLengthPixels = 88.0f;
	static constexpr float HitRadiusPixels = 8.0f;
	static constexpr float CenterHandleRadiusPixels = 17.0f;
	static constexpr float DragDeadZonePixels = 2.0f;

	EGizmoViewMode Mode = EGizmoViewMode::Translate;
	bool bLocalSpace = false;
	FMatrix AxisRotation = FMatrix::Identity;
	ETransformGizmoAxis HoveredAxis = ETransformGizmoAxis::None;
	ETransformGizmoAxis ActiveAxis = ETransformGizmoAxis::None;

	// 기즈모 드래그 조작 시 시작값 대비 결과값을 계산하기 위해 사용하는 파라미터.
	TArray<FDragTarget> DragTargets;
	TArray<uint32> DragSelectionUUIDs;
	FVector StartWorldOrigin;
	FVector StartRotationVector; // 축 회전 평면 또는 Trackball 가상 구에서 구한 드래그 시작 방향.
	FVector StartPlanePosition; // 자유 이동을 시작할 때 Cursor Ray와 View 평면이 교차한 World Position.
	float StartAxisParameter = 0.0f; // 이동/스케일 기즈모 조작 시 Ray에 가장 가까운 축 위의 위치.

	FVector2 DragStartPixel = FVector2::ZeroVector; // Dead Zone과 Uniform Scale Delta 계산에 사용하는 드래그 시작 Pixel 좌표.
	FVector DragRotationAxis; // 축 회전 또는 View 회전에 사용하는 World Space 회전축.
	uint32 DragPivotUUID = 0; // 마지막 활성 선택이며 기즈모가 표시되는 Pivot Node의 UUID.
	bool bDragging = false;
};
