#include "TransformGizmo.h"

#include "Component/TransformComponent.h"
#include "Core/Geometry/Ray.h"
#include "Core/Math/Math.h"
#include "Core/Math/Matrix.h"
#include "Editor/EditorSelection.h"
#include "Input/InputRouter.h"
#include "Object/Object.h"
#include "World/Node.h"

#include <algorithm>
#include <cmath>
#include <limits>

// 키보드 모드 전환과 Hover, 드래그, 확정 및 취소 입력을 처리한다.
FInputReply FTransformGizmo::OnInputEvent(const FInputEvent& Event, const FEditorSelection& Selection, const FSceneView& View, const FVector2& PixelPosition)
{
	if (const FKeyInputEvent* KeyEvent = std::get_if<FKeyInputEvent>(&Event))
	{
		if (bDragging && KeyEvent->bDown && !KeyEvent->bRepeat && KeyEvent->Key == EKeyboardKey::Escape)
		{
			CancelDrag();
			return FInputReply::Handled().ReleaseMouse();
		}
		if (!bDragging && Selection.SelectedNode && KeyEvent->bDown && !KeyEvent->bRepeat && KeyEvent->Key == EKeyboardKey::Space)
		{
			if (Mode == EGizmoViewMode::Translate) Mode = EGizmoViewMode::Rotate;
			else if (Mode == EGizmoViewMode::Rotate) Mode = EGizmoViewMode::Scale;
			else Mode = EGizmoViewMode::Translate;
			return FInputReply::Handled();
		}
		return FInputReply::Unhandled();
	}

	const FPointerInputEvent* PointerEvent = std::get_if<FPointerInputEvent>(&Event);
	if (!PointerEvent)
	{
		return FInputReply::Unhandled();
	}
	if (bDragging)
	{
		if (PointerEvent->Type == EPointerInputEventType::ButtonUp && PointerEvent->Button == EMouseButton::Left)
		{
			EndDrag();
			return FInputReply::Handled().ReleaseMouse();
		}
		if (PointerEvent->Type == EPointerInputEventType::ButtonDown && PointerEvent->Button == EMouseButton::Right)
		{
			CancelDrag();
			return FInputReply::Handled().ReleaseMouse();
		}
		if (PointerEvent->Type == EPointerInputEventType::CursorMoved)
		{
			UpdateDrag(Selection, View, PixelPosition);
			return FInputReply::Handled();
		}
		return FInputReply::Handled();
	}

	UNode* SelectedNode = Selection.SelectedNode;
	if (!SelectedNode)
	{
		HoveredAxis = ETransformGizmoAxis::None;
		return FInputReply::Unhandled();
	}
	if (PointerEvent->Type == EPointerInputEventType::CursorMoved)
	{
		HoveredAxis = HitTest(View, PixelPosition, SelectedNode->GetTransform().GetWorldLocation());
		return HoveredAxis == ETransformGizmoAxis::None ? FInputReply::Unhandled() : FInputReply::Handled();
	}
	if (PointerEvent->Type == EPointerInputEventType::ButtonDown && PointerEvent->Button == EMouseButton::Left)
	{
		const ETransformGizmoAxis Axis = HitTest(View, PixelPosition, SelectedNode->GetTransform().GetWorldLocation());
		if (Axis != ETransformGizmoAxis::None && BeginDrag(Selection, Axis, View, PixelPosition))
		{
			return FInputReply::Handled().SetKeyboardFocus().CaptureMouse();
		}
	}
	return FInputReply::Unhandled();
}

// 비정상적인 캡처 상실은 시작 Transform으로 복원한다.
void FTransformGizmo::OnMouseCaptureLost()
{
	if (bDragging)
	{
		CancelDrag();
	}
}

// 키보드 포커스를 잃은 진행 중 조작을 취소한다.
void FTransformGizmo::OnKeyboardFocusLost()
{
	if (bDragging)
	{
		CancelDrag();
	}
}

// 좌표계를 전환하고 이전 축의 Hover 상태를 초기화한다.
void FTransformGizmo::SetLocalSpace(bool bInLocalSpace)
{
	if (bDragging)
	{
		return;
	}
	bLocalSpace = bInLocalSpace;
	HoveredAxis = ETransformGizmoAxis::None;
}

// 선택 변경을 반영하고 드래그 중이 아닐 때 선택 객체의 회전 축을 갱신한다.
void FTransformGizmo::UpdateSelection(const FEditorSelection& Selection)
{
	if (bDragging && !IsDragSelectionValid(Selection))
	{
		CancelDrag();
	}
	if (!bDragging)
	{
		AxisRotation = FMatrix::Identity;
		if (bLocalSpace && Selection.SelectedNode)
		{
			for (const UTransformComponent* Transform = &Selection.SelectedNode->GetTransform(); Transform; Transform = Transform->GetParent())
			{
				AxisRotation *= Transform->GetRelativeTransform().Rotation.ToMatrix();
			}
		}
	}
}

// 현재 선택과 카메라로 일정한 화면 크기의 Render Thread 전달 값을 만든다.
void FTransformGizmo::BuildGizmoView(const FEditorSelection& Selection, const FSceneView& View, FGizmoView& OutData) const
{
	if (!Selection.SelectedNode)
	{
		OutData = {};
		return;
	}
	OutData.Origin = Selection.SelectedNode->GetTransform().GetWorldLocation();
	OutData.WorldScale = GetUnitsPerPixel(View, OutData.Origin) * GizmoLengthPixels;
	OutData.Mode = Mode;
	OutData.AxisX = GetAxisVector(ETransformGizmoAxis::X);
	OutData.AxisY = GetAxisVector(ETransformGizmoAxis::Y);
	OutData.AxisZ = GetAxisVector(ETransformGizmoAxis::Z);
	OutData.HighlightedAxis = static_cast<int32>(bDragging ? ActiveAxis : HoveredAxis);
	OutData.bVisible = OutData.WorldScale > KMath::Epsilon;
}

// 축 열거형을 Knot의 월드 좌표계 단위 벡터로 변환한다.
FVector FTransformGizmo::GetAxisVector(ETransformGizmoAxis Axis) const
{
	switch (Axis)
	{
	case ETransformGizmoAxis::X: return AxisRotation.GetScaledAxis(EAxis::X);
	case ETransformGizmoAxis::Y: return AxisRotation.GetScaledAxis(EAxis::Y);
	case ETransformGizmoAxis::Z: return AxisRotation.GetScaledAxis(EAxis::Z);
	case ETransformGizmoAxis::View:
	case ETransformGizmoAxis::Trackball:
	case ETransformGizmoAxis::Center:
	case ETransformGizmoAxis::None: break;
	}
	return FVector::ZeroVector;
}

// View 회전 Handle이 사용하는 Gizmo 중심에서 카메라를 향하는 축을 반환한다.
FVector FTransformGizmo::GetViewRotationAxis(const FSceneView& View, const FVector& Origin)
{
	return (View.ViewOrigin - Origin).GetSafeNormal();
}

// 커서 위치를 View 방향을 향한 가상 구 위의 월드 벡터로 변환한다.
FVector FTransformGizmo::MapTrackballVector(const FSceneView& View, const FVector& Origin, const FVector2& PixelPosition)
{
	FVector2 CenterPixel;
	if (!WorldToScreen(View, Origin, CenterPixel))
	{
		return FVector::ZeroVector;
	}

	float X = (PixelPosition.X - CenterPixel.X) / GizmoLengthPixels;
	float Y = (CenterPixel.Y - PixelPosition.Y) / GizmoLengthPixels;
	const float LengthSquared = X * X + Y * Y;
	float Z = 0.0f;
	if (LengthSquared > 1.0f)
	{
		const float InverseLength = 1.0f / std::sqrt(LengthSquared);
		X *= InverseLength;
		Y *= InverseLength;
	}
	else
	{
		Z = std::sqrt(1.0f - LengthSquared);
	}

	const FMatrix InverseView = View.ViewMatrix.GetInverse();
	const FVector ViewRight = InverseView.GetScaledAxis(EAxis::X).GetSafeNormal();
	const FVector ViewUp = InverseView.GetScaledAxis(EAxis::Y).GetSafeNormal();
	const FVector ViewForward = GetViewRotationAxis(View, Origin);
	return (ViewRight * X + ViewUp * Y + ViewForward * Z).GetSafeNormal();
}

// 원근과 직교 View에서 한 논리 Render Target Pixel이 차지하는 월드 길이를 계산한다.
float FTransformGizmo::GetUnitsPerPixel(const FSceneView& View, const FVector& WorldPosition)
{
	if (View.Viewport.Height <= 0.0f || std::fabs(View.ProjectionMatrix.M[1][1]) <= KMath::Epsilon)
	{
		return 0.0f;
	}
	const bool bPerspective = std::fabs(View.ProjectionMatrix.M[2][3]) > KMath::Epsilon;
	const FVector4 ViewPosition = FVector4(WorldPosition, 1.0f) * View.ViewMatrix;
	const float Distance = bPerspective ? ViewPosition.Z : 1.0f;
	if (Distance <= KMath::Epsilon)
	{
		return 0.0f;
	}
	return Distance * 2.0f / (std::fabs(View.ProjectionMatrix.M[1][1]) * View.Viewport.Height);
}

// 월드 위치를 현재 Viewport의 Pixel 좌표로 투영한다.
bool FTransformGizmo::WorldToScreen(const FSceneView& View, const FVector& WorldPosition, FVector2& OutPixelPosition)
{
	const FVector4 Clip = FVector4(WorldPosition, 1.0f) * View.ViewProjectionMatrix;
	if (Clip.W <= KMath::Epsilon)
	{
		return false;
	}
	OutPixelPosition.X = (Clip.X / Clip.W * 0.5f + 0.5f) * View.Viewport.Width;
	OutPixelPosition.Y = (0.5f - Clip.Y / Clip.W * 0.5f) * View.Viewport.Height;
	return true;
}

// 화면 점과 선분 사이의 제곱 거리를 반환한다.
float FTransformGizmo::DistanceToSegmentSquared(const FVector2& Point, const FVector2& Start, const FVector2& End)
{
	const FVector2 Segment = End - Start;
	const float LengthSquared = Segment.X * Segment.X + Segment.Y * Segment.Y;
	if (LengthSquared <= KMath::Epsilon)
	{
		const FVector2 Delta = Point - Start;
		return Delta.X * Delta.X + Delta.Y * Delta.Y;
	}
	const FVector2 ToPoint = Point - Start;
	const float Alpha = std::clamp((ToPoint.X * Segment.X + ToPoint.Y * Segment.Y) / LengthSquared, 0.0f, 1.0f);
	const FVector2 Delta = Point - (Start + Segment * Alpha);
	return Delta.X * Delta.X + Delta.Y * Delta.Y;
}

// Ray와 무한 축 사이의 최단점에서 축 방향 매개변수를 구한다.
bool FTransformGizmo::IntersectAxis(const FRay& Ray, const FVector& Origin, const FVector& Axis, float& OutAxisParameter)
{
	const FVector Offset = Ray.Origin - Origin;
	const float Parallel = Ray.Direction | Axis;
	const float Denominator = 1.0f - Parallel * Parallel;
	if (Denominator <= 1.0e-4f)
	{
		return false;
	}
	OutAxisParameter = ((Offset | Axis) - Parallel * (Offset | Ray.Direction)) / Denominator;
	return true;
}

// Ray와 평면의 교차점을 계산한다.
bool FTransformGizmo::IntersectPlane(const FRay& Ray, const FVector& PlaneOrigin, const FVector& PlaneNormal, FVector& OutPosition)
{
	const float Denominator = Ray.Direction | PlaneNormal;
	if (std::fabs(Denominator) <= 1.0e-4f)
	{
		return false;
	}
	const float Distance = ((PlaneOrigin - Ray.Origin) | PlaneNormal) / Denominator;
	if (Distance < 0.0f)
	{
		return false;
	}
	OutPosition = Ray.Origin + Ray.Direction * Distance;
	return true;
}

// 화면 공간 축 선분 또는 회전 원에 가장 가까운 조작 축을 찾는다.
ETransformGizmoAxis FTransformGizmo::HitTest(const FSceneView& View, const FVector2& PixelPosition, const FVector& Origin) const
{
	const float WorldScale = GetUnitsPerPixel(View, Origin) * GizmoLengthPixels;
	const float HitDistanceSquared = HitRadiusPixels * HitRadiusPixels;
	float ClosestDistanceSquared = HitDistanceSquared;
	ETransformGizmoAxis ClosestAxis = ETransformGizmoAxis::None;
	FVector2 CenterPixel;
	const bool bHasCenterPixel = WorldToScreen(View, Origin, CenterPixel);
	const FMatrix InverseView = View.ViewMatrix.GetInverse();
	const FVector ViewRight = InverseView.GetScaledAxis(EAxis::X).GetSafeNormal();
	const FVector ViewUp = InverseView.GetScaledAxis(EAxis::Y).GetSafeNormal();
	FVector2 RotationRightDelta = FVector2::ZeroVector;
	FVector2 RotationUpDelta = FVector2::ZeroVector;
	if (Mode == EGizmoViewMode::Rotate && bHasCenterPixel)
	{
		FVector2 RightPixel;
		FVector2 UpPixel;
		if (!WorldToScreen(View, Origin + ViewRight * WorldScale, RightPixel) || !WorldToScreen(View, Origin + ViewUp * WorldScale, UpPixel))
		{
			return ETransformGizmoAxis::None;
		}
		RotationRightDelta = RightPixel - CenterPixel;
		RotationUpDelta = UpPixel - CenterPixel;
	}

	for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
	{
		const auto Axis = static_cast<ETransformGizmoAxis>(AxisIndex);
		const FVector AxisVector = GetAxisVector(Axis);
		if (Mode != EGizmoViewMode::Rotate)
		{
			FVector2 StartPixel;
			FVector2 EndPixel;
			if (!WorldToScreen(View, Origin + AxisVector * (WorldScale * 0.14f), StartPixel) ||
				!WorldToScreen(View, Origin + AxisVector * WorldScale, EndPixel))
			{
				continue;
			}
			const float DistanceSquared = DistanceToSegmentSquared(PixelPosition, StartPixel, EndPixel);
			if (DistanceSquared < ClosestDistanceSquared)
			{
				ClosestDistanceSquared = DistanceSquared;
				ClosestAxis = Axis;
			}
			continue;
		}

		FVector Side0;
		FVector Side1;
		if (Axis == ETransformGizmoAxis::X)
		{
			Side0 = GetAxisVector(ETransformGizmoAxis::Y);
			Side1 = GetAxisVector(ETransformGizmoAxis::Z);
		}
		else if (Axis == ETransformGizmoAxis::Y)
		{
			Side0 = GetAxisVector(ETransformGizmoAxis::X);
			Side1 = GetAxisVector(ETransformGizmoAxis::Z);
		}
		else
		{
			Side0 = GetAxisVector(ETransformGizmoAxis::X);
			Side1 = GetAxisVector(ETransformGizmoAxis::Y);
		}
		for (uint32 Segment = 0; Segment < 64; ++Segment)
		{
			const float Angle0 = 2.0f * KMath::Pi * static_cast<float>(Segment) / 64.0f;
			const float Angle1 = 2.0f * KMath::Pi * static_cast<float>(Segment + 1) / 64.0f;
			const FVector Start = Side0 * std::cos(Angle0) + Side1 * std::sin(Angle0);
			const FVector End = Side0 * std::cos(Angle1) + Side1 * std::sin(Angle1);
			if ((((Start + End) * 0.5f) | (View.ViewOrigin - Origin)) <= 0.0f)
			{
				continue;
			}
			const FVector2 StartPixel = CenterPixel + RotationRightDelta * (Start | ViewRight) + RotationUpDelta * (Start | ViewUp);
			const FVector2 EndPixel = CenterPixel + RotationRightDelta * (End | ViewRight) + RotationUpDelta * (End | ViewUp);
			const float DistanceSquared = DistanceToSegmentSquared(PixelPosition, StartPixel, EndPixel);
			if (DistanceSquared < ClosestDistanceSquared)
			{
				ClosestDistanceSquared = DistanceSquared;
				ClosestAxis = Axis;
			}
		}
	}
	if (ClosestAxis != ETransformGizmoAxis::None)
	{
		return ClosestAxis;
	}

	if (!bHasCenterPixel)
	{
		return ETransformGizmoAxis::None;
	}
	const FVector2 CenterDelta = PixelPosition - CenterPixel;
	const float CursorRadius = std::sqrt(CenterDelta.X * CenterDelta.X + CenterDelta.Y * CenterDelta.Y);
	if (Mode != EGizmoViewMode::Rotate)
	{
		return CursorRadius <= CenterHandleRadiusPixels ? ETransformGizmoAxis::Center : ETransformGizmoAxis::None;
	}

	const FVector2 RingPixel = CenterPixel + RotationRightDelta * 1.1325f;
	const FVector2 RadiusDelta = RingPixel - CenterPixel;
	const float RingRadius = std::sqrt(RadiusDelta.X * RadiusDelta.X + RadiusDelta.Y * RadiusDelta.Y);
	if (std::fabs(CursorRadius - RingRadius) <= HitRadiusPixels)
	{
		return ETransformGizmoAxis::View;
	}
	if (CursorRadius < RingRadius - HitRadiusPixels)
	{
		return ETransformGizmoAxis::Trackball;
	}
	return ETransformGizmoAxis::None;
}

// 축 위의 시작 교차점부터 현재 교차점까지의 월드 이동량을 적용한다.
void FTransformGizmo::ApplyTranslation(const FSceneView& View, const FVector2& PixelPosition)
{
	const FRay Ray = FRay::BuildRay(PixelPosition.X, PixelPosition.Y, View.ViewProjectionMatrix, View.Viewport.Width, View.Viewport.Height);
	FVector WorldPosition;
	if (ActiveAxis == ETransformGizmoAxis::Center)
	{
		FVector PlanePosition;
		if (!IntersectPlane(Ray, StartWorldOrigin, DragRotationAxis, PlanePosition))
		{
			return;
		}
		WorldPosition = StartWorldOrigin + PlanePosition - StartPlanePosition;
	}
	else
	{
		const FVector Axis = GetAxisVector(ActiveAxis);
		float AxisParameter = 0.0f;
		if (!IntersectAxis(Ray, StartWorldOrigin, Axis, AxisParameter))
		{
			return;
		}
		WorldPosition = StartWorldOrigin + Axis * (AxisParameter - StartAxisParameter);
	}
	if (!ApplyWorldDelta(FMatrix::MakeTranslation(WorldPosition - StartWorldOrigin)))
	{
		CancelDrag();
	}
}

// 회전 평면의 시작 벡터와 현재 벡터 사이 각도로 시작 World Transform을 회전한다.
void FTransformGizmo::ApplyRotation(const FSceneView& View, const FVector2& PixelPosition)
{
	FVector RotationAxis = DragRotationAxis;
	FVector CurrentVector;
	if (ActiveAxis == ETransformGizmoAxis::Trackball)
	{
		CurrentVector = MapTrackballVector(View, StartWorldOrigin, PixelPosition);
		RotationAxis = (StartRotationVector ^ CurrentVector).GetSafeNormal();
	}
	else
	{
		const FRay Ray = FRay::BuildRay(PixelPosition.X, PixelPosition.Y, View.ViewProjectionMatrix, View.Viewport.Width, View.Viewport.Height);
		FVector Position;
		if (!IntersectPlane(Ray, StartWorldOrigin, RotationAxis, Position))
		{
			return;
		}
		CurrentVector = (Position - StartWorldOrigin).GetSafeNormal();
	}
	if (CurrentVector.IsNearlyZero())
	{
		return;
	}
	const FVector Cross = StartRotationVector ^ CurrentVector;
	const float SinAngle = ActiveAxis == ETransformGizmoAxis::Trackball ? Cross.Size() : RotationAxis | Cross;
	const float CosAngle = std::clamp(StartRotationVector | CurrentVector, -1.0f, 1.0f);
	const float Angle = std::atan2(SinAngle, CosAngle);
	if (RotationAxis.IsNearlyZero() || std::fabs(Angle) <= KMath::Epsilon)
	{
		return;
	}
	const FMatrix DeltaRotation = FQuat(RotationAxis, Angle).ToMatrix();
	if (!ApplyWorldDelta(FMatrix::MakeTranslation(-StartWorldOrigin) * DeltaRotation * FMatrix::MakeTranslation(StartWorldOrigin)))
	{
		CancelDrag();
	}
}

// 축 위의 시작 교차점부터 현재 교차점까지의 이동을 시작 Scale의 축 배율로 변환한다.
void FTransformGizmo::ApplyScale(const FSceneView& View, const FVector2& PixelPosition)
{
	float Factor = 1.0f;
	if (ActiveAxis == ETransformGizmoAxis::Center)
	{
		const FVector2 PixelDelta = PixelPosition - DragStartPixel;
		Factor = std::max(0.01f, 1.0f + (PixelDelta.X - PixelDelta.Y) / GizmoLengthPixels);
	}
	else
	{
		const FVector Axis = GetAxisVector(ActiveAxis);
		const FRay Ray = FRay::BuildRay(PixelPosition.X, PixelPosition.Y, View.ViewProjectionMatrix, View.Viewport.Width, View.Viewport.Height);
		float AxisParameter = 0.0f;
		if (!IntersectAxis(Ray, StartWorldOrigin, Axis, AxisParameter))
		{
			return;
		}
		const float GizmoLength = GetUnitsPerPixel(View, StartWorldOrigin) * GizmoLengthPixels;
		if (GizmoLength <= KMath::Epsilon)
		{
			return;
		}
		Factor = std::max(0.01f, 1.0f + (AxisParameter - StartAxisParameter) / GizmoLength);
	}
	FVector Scale = FVector::OneVector;
	if (ActiveAxis == ETransformGizmoAxis::Center)
	{
		Scale *= Factor;
	}
	else
	{
		Scale[static_cast<int32>(ActiveAxis)] = Factor;
	}
	const FMatrix WorldDelta = FMatrix::MakeTranslation(-StartWorldOrigin) * AxisRotation.GetInverse() * FMatrix::MakeScale(Scale) * AxisRotation *
		FMatrix::MakeTranslation(StartWorldOrigin);
	if (!ApplyWorldDelta(WorldDelta))
	{
		CancelDrag();
	}
}

// 모든 실제 조작 대상의 시작 World Transform에 동일한 Pivot 기준 Delta를 적용한다.
bool FTransformGizmo::ApplyWorldDelta(const FMatrix& WorldDelta)
{
	struct FPendingTransform
	{
		UNode* Node = nullptr;
		FTransform RelativeTransform;
	};

	TArray<FPendingTransform> PendingTransforms;
	PendingTransforms.reserve(DragTargets.size());
	for (const FDragTarget& Target : DragTargets)
	{
		UNode* Node = ResolveNode(Target.NodeUUID);
		if (!Node)
		{
			return false;
		}

		const FMatrix RelativeMatrix = Target.StartWorldMatrix * WorldDelta * Target.StartParentWorldInverse;
		FVector Translation;
		FVector Scale;
		FMatrix Rotation;
		if (!RelativeMatrix.Decompose(Translation, Rotation, Scale))
		{
			return false;
		}
		PendingTransforms.push_back({ Node, FTransform(FQuat(Rotation), Translation, Scale) });
	}

	for (const FPendingTransform& Pending : PendingTransforms)
	{
		Pending.Node->GetTransform().SetRelativeTransform(Pending.RelativeTransform);
	}
	return true;
}

// 취소 또는 Dead Zone 복귀 시 모든 실제 조작 대상을 드래그 시작 상태로 되돌린다.
void FTransformGizmo::RestoreDragTargets()
{
	for (const FDragTarget& Target : DragTargets)
	{
		if (UNode* Node = ResolveNode(Target.NodeUUID))
		{
			Node->GetTransform().SetRelativeTransform(Target.StartRelativeTransform);
		}
	}
}

// 조작 시작 시 대상과 기준 Transform 및 기하 교차값을 고정한다.
bool FTransformGizmo::BeginDrag(const FEditorSelection& Selection, ETransformGizmoAxis Axis, const FSceneView& View, const FVector2& PixelPosition)
{
	UNode* PivotNode = Selection.SelectedNode;
	if (!PivotNode)
	{
		return false;
	}
	StartWorldOrigin = PivotNode->GetTransform().GetWorldLocation();
	DragStartPixel = PixelPosition;
	DragPivotUUID = PivotNode->GetUUID();
	ActiveAxis = Axis;
	HoveredAxis = Axis;

	const FRay Ray = FRay::BuildRay(PixelPosition.X, PixelPosition.Y, View.ViewProjectionMatrix, View.Viewport.Width, View.Viewport.Height);
	if (Mode == EGizmoViewMode::Rotate)
	{
		if (Axis == ETransformGizmoAxis::Trackball)
		{
			StartRotationVector = MapTrackballVector(View, StartWorldOrigin, PixelPosition);
			if (StartRotationVector.IsNearlyZero())
			{
				return false;
			}
		}
		else
		{
			DragRotationAxis = Axis == ETransformGizmoAxis::View ? GetViewRotationAxis(View, StartWorldOrigin) : GetAxisVector(Axis);
			FVector Position;
			if (!IntersectPlane(Ray, StartWorldOrigin, DragRotationAxis, Position))
			{
				return false;
			}
			StartRotationVector = (Position - StartWorldOrigin).GetSafeNormal();
			if (StartRotationVector.IsNearlyZero())
			{
				return false;
			}
		}
	}
	else if (Axis == ETransformGizmoAxis::Center && Mode == EGizmoViewMode::Translate)
	{
		DragRotationAxis = GetViewRotationAxis(View, StartWorldOrigin);
		if (!IntersectPlane(Ray, StartWorldOrigin, DragRotationAxis, StartPlanePosition))
		{
			return false;
		}
	}
	else if (Axis == ETransformGizmoAxis::Center)
	{
		StartAxisParameter = 0.0f;
	}
	else if (!IntersectAxis(Ray, StartWorldOrigin, GetAxisVector(Axis), StartAxisParameter))
	{
		StartAxisParameter = 0.0f;
	}

	DragTargets.clear();
	DragSelectionUUIDs.clear();
	const TArray<UNode*>& SelectedNodes = Selection.GetSelectedNodes();
	DragSelectionUUIDs.reserve(SelectedNodes.size());
	DragTargets.reserve(SelectedNodes.size());
	for (UNode* Node : SelectedNodes)
	{
		if (!Node)
		{
			continue;
		}
		DragSelectionUUIDs.push_back(Node->GetUUID());

		bool bHasSelectedAncestor = false;
		for (UTransformComponent* Parent = Node->GetTransform().GetParent(); Parent; Parent = Parent->GetParent())
		{
			UNode* ParentNode = &Parent->GetOwner();
			if (std::find(SelectedNodes.begin(), SelectedNodes.end(), ParentNode) != SelectedNodes.end())
			{
				bHasSelectedAncestor = true;
				break;
			}
		}
		if (bHasSelectedAncestor)
		{
			continue;
		}

		FDragTarget Target;
		Target.NodeUUID = Node->GetUUID();
		Target.StartRelativeTransform = Node->GetTransform().GetRelativeTransform();
		Target.StartWorldMatrix = Node->GetTransform().GetWorldMatrix();
		if (UTransformComponent* Parent = Node->GetTransform().GetParent())
		{
			const FMatrix ParentWorldMatrix = Parent->GetWorldMatrix();
			if (std::fabs(ParentWorldMatrix.GetDeterminant()) <= KMath::Epsilon)
			{
				DragTargets.clear();
				DragSelectionUUIDs.clear();
				DragPivotUUID = 0;
				return false;
			}
			Target.StartParentWorldInverse = ParentWorldMatrix.GetInverse();
		}
		DragTargets.push_back(Target);
	}
	if (DragTargets.empty())
	{
		DragSelectionUUIDs.clear();
		DragPivotUUID = 0;
		return false;
	}
	bDragging = true;
	return true;
}

// 시작점 Dead Zone을 넘은 커서 목표로부터 시작 Transform 기준 결과를 다시 계산한다.
void FTransformGizmo::UpdateDrag(const FEditorSelection& Selection, const FSceneView& View, const FVector2& PixelPosition)
{
	if (!IsDragSelectionValid(Selection))
	{
		CancelDrag();
		return;
	}
	FVector2 TargetPixelPosition = PixelPosition;
	if (Mode == EGizmoViewMode::Translate || Mode == EGizmoViewMode::Scale)
	{
		TargetPixelPosition.X = std::clamp(TargetPixelPosition.X, 0.0f, static_cast<float>(View.Viewport.Width));
		TargetPixelPosition.Y = std::clamp(TargetPixelPosition.Y, 0.0f, static_cast<float>(View.Viewport.Height));
	}
	const FVector2 PixelDelta = TargetPixelPosition - DragStartPixel;
	if (PixelDelta.X * PixelDelta.X + PixelDelta.Y * PixelDelta.Y <= DragDeadZonePixels * DragDeadZonePixels)
	{
		RestoreDragTargets();
		return;
	}
	if (Mode == EGizmoViewMode::Translate)
	{
		ApplyTranslation(View, TargetPixelPosition);
	}
	else if (Mode == EGizmoViewMode::Rotate)
	{
		ApplyRotation(View, PixelPosition);
	}
	else
	{
		ApplyScale(View, TargetPixelPosition);
	}
}

// 취소 가능한 대상이 남아 있으면 시작 Transform을 복원하고 상태를 해제한다.
void FTransformGizmo::CancelDrag()
{
	RestoreDragTargets();
	EndDrag();
}

// 확정된 Transform은 유지하고 드래그 상태만 해제한다.
void FTransformGizmo::EndDrag()
{
	bDragging = false;
	ActiveAxis = ETransformGizmoAxis::None;
	DragPivotUUID = 0;
	DragTargets.clear();
	DragSelectionUUIDs.clear();
}

// UUID로 현재도 살아 있는 Node를 안전하게 다시 찾는다.
UNode* FTransformGizmo::ResolveNode(uint32 NodeUUID) const
{
	UObject* Object = NodeUUID != 0 ? GUObjectManager.FindByUUID(NodeUUID) : nullptr;
	return Object && Object->IsA(UNode::StaticClass()) ? static_cast<UNode*>(Object) : nullptr;
}

// 드래그 시작 당시의 마지막 활성 선택과 전체 선택 목록이 그대로 유지되는지 검사한다.
bool FTransformGizmo::IsDragSelectionValid(const FEditorSelection& Selection) const
{
	if (!Selection.SelectedNode || Selection.SelectedNode->GetUUID() != DragPivotUUID)
	{
		return false;
	}
	const TArray<UNode*>& SelectedNodes = Selection.GetSelectedNodes();
	if (SelectedNodes.size() != DragSelectionUUIDs.size())
	{
		return false;
	}
	for (SIZE_T Index = 0; Index < SelectedNodes.size(); ++Index)
	{
		if (!SelectedNodes[Index] || SelectedNodes[Index]->GetUUID() != DragSelectionUUIDs[Index])
		{
			return false;
		}
	}
	return ResolveNode(DragPivotUUID) == Selection.SelectedNode;
}
