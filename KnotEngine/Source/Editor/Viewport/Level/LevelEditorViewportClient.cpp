#include "LevelEditorViewportClient.h"

#include "Core/Geometry/Ray.h"
#include "Core/Math/Matrix.h"
#include "Component/Mesh/StaticMeshComponent.h"
#include "Component/TransformComponent.h"
#include "Editor/Context/EditorSelection.h"
#include "Runtime/EditorEngine.h"
#include "Viewport/Viewport.h"
#include "World/World.h"
#include "World/Level.h"
#include "World/Node.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <variant>

FLevelEditorViewportClient::FLevelEditorViewportClient(FViewport& InViewport)
	: FEditorViewportClient(InViewport), Selection(GetEditor().GetEditorSelection()),
	  TransformGizmo(GetEditor().GetEditorTransaction())
{
}

// Level Editor가 소유한 Editor World를 반환한다.
UWorld* FLevelEditorViewportClient::GetWorld() const
{
	return GetEditor().GetWorld();
}

// 입력 이벤트가 없는 프레임에도 선택 변경과 대상 제거를 반영하고 일반 Viewport Tick을 수행한다.
void FLevelEditorViewportClient::Tick(float DeltaTime)
{
	ApplySelectionResult();
	ApplyPlacementResult();
	TransformGizmo.UpdateSelection(Selection);
	FEditorViewportClient::Tick(DeltaTime);
	UpdateFocusAnimation(DeltaTime);
}

// 진행 중인 카메라 드래그를 우선 처리하고 일반 왼쪽 클릭은 장면 선택으로 전달한다.
FInputReply FLevelEditorViewportClient::OnInputEvent(const FInputEvent& Event)
{
	TransformGizmo.UpdateSelection(Selection);
	const FPointerInputEvent* PointerEvent = std::get_if<FPointerInputEvent>(&Event);
	if (PointerEvent && bTrackingRightClick && PointerEvent->Type == EPointerInputEventType::MouseMoved)
	{
		RightClickTravelSquared += PointerEvent->Delta.SizeSquared();
	}

	if (IsCameraDragging())
	{
		const bool bRightButtonReleased = PointerEvent && PointerEvent->Type == EPointerInputEventType::ButtonUp &&
		                                  PointerEvent->Button == EMouseButton::Right;
		const bool bOpenContextMenu = bRightButtonReleased && bTrackingRightClick &&
		                              RightClickTravelSquared <= ContextMenuDragThresholdSquared && PointerEvent->Modifiers == EModifierKeyMask::None;
		const FInputReply Reply = FEditorViewportClient::OnInputEvent(Event);
		if (bRightButtonReleased)
		{
			bTrackingRightClick = false;
			if (bOpenContextMenu)
			{
				RequestContextMenu(RightClickStartPosition);
			}
		}
		return Reply;
	}
	if (const FKeyInputEvent* KeyEvent = std::get_if<FKeyInputEvent>(&Event))
	{
		if (bTrackingBoxSelection && KeyEvent->bDown && !KeyEvent->bRepeat && KeyEvent->Key == EKeyboardKey::Escape)
		{
			bTrackingBoxSelection = false;
			bBoxSelecting = false;
			return FInputReply::Handled().ReleaseMouse();
		}
	}
	if (PointerEvent && bTrackingBoxSelection)
	{
		if (PointerEvent->Type == EPointerInputEventType::CursorMoved)
		{
			BoxSelectionEnd = PointerEvent->Position;
			bBoxSelecting = FVector2::DistSquared(BoxSelectionStart, BoxSelectionEnd) >= BoxSelectionDragThresholdSquared;
			return FInputReply::Handled();
		}
		if (PointerEvent->Type == EPointerInputEventType::ButtonUp && PointerEvent->Button == EMouseButton::Left)
		{
			BoxSelectionEnd = PointerEvent->Position;
			bBoxSelecting = FVector2::DistSquared(BoxSelectionStart, BoxSelectionEnd) >= BoxSelectionDragThresholdSquared;
			if (bBoxSelecting)
			{
				ApplyBoxSelection();
			}
			else
			{
				RequestSelection(BoxSelectionStart, BoxSelectionModifiers);
			}
			bTrackingBoxSelection = false;
			bBoxSelecting = false;
			return FInputReply::Handled().ReleaseMouse();
		}
		return FInputReply::Handled();
	}

	if (PointerEvent && (PointerEvent->Type == EPointerInputEventType::ButtonDown || PointerEvent->Type == EPointerInputEventType::Wheel))
	{
		bFocusAnimating = false;
	}
	FVector2 PixelPosition = FVector2::ZeroVector;

	if (PointerEvent && !GetViewportPixelPosition(PointerEvent->Position, PixelPosition))
	{
		return FInputReply::Unhandled();
	}

	const FInputReply GizmoReply = TransformGizmo.OnInputEvent(Event, Selection, BuildSceneView(), PixelPosition);
	if (GizmoReply.IsHandled())
	{
		if (PointerEvent && PointerEvent->Type == EPointerInputEventType::ButtonDown)
		{
			PendingRayQueries.clear();
			PendingPlacementQuery.reset();
		}
		return GizmoReply;
	}
	if (PointerEvent && PointerEvent->Type == EPointerInputEventType::ButtonDown && PointerEvent->Button == EMouseButton::Right)
	{
		RightClickStartPosition = PointerEvent->Position;
		RightClickTravelSquared = 0.0f;
		bTrackingRightClick = true;
	}
	if (PointerEvent && PointerEvent->Type == EPointerInputEventType::ButtonUp && PointerEvent->Button == EMouseButton::Right)
	{
		const bool bOpenContextMenu = bTrackingRightClick && RightClickTravelSquared <= ContextMenuDragThresholdSquared &&
		                              PointerEvent->Modifiers == EModifierKeyMask::None;
		bTrackingRightClick = false;
		if (bOpenContextMenu)
		{
			RequestContextMenu(RightClickStartPosition);
		}
	}

	if (const FKeyInputEvent* KeyEvent = std::get_if<FKeyInputEvent>(&Event))
	{
		if (KeyEvent->Key == EKeyboardKey::Escape && KeyEvent->bDown && (bFocusAnimating || !PendingRayQueries.empty() || PendingPlacementQuery))
		{
			bFocusAnimating = false;
			PendingRayQueries.clear();
			PendingPlacementQuery.reset();
			return FInputReply::Handled();
		}
		if (KeyEvent->Key == EKeyboardKey::F && KeyEvent->Modifiers == EModifierKeyMask::None)
		{
			if (KeyEvent->bDown && !KeyEvent->bRepeat && !TransformGizmo.IsDragging())
			{
				FocusSelectedNode();
			}
			return FInputReply::Handled();
		}
	}

	if (PointerEvent && PointerEvent->Type == EPointerInputEventType::ButtonDown && PointerEvent->Button == EMouseButton::Left)
	{
		BoxSelectionStart = PointerEvent->Position;
		BoxSelectionEnd = PointerEvent->Position;
		BoxSelectionModifiers = PointerEvent->Modifiers;
		bTrackingBoxSelection = true;
		bBoxSelecting = false;
		return FInputReply::Handled().SetKeyboardFocus().CaptureMouse();
	}

	return FEditorViewportClient::OnInputEvent(Event);
}

// 키보드 포커스를 잃으면 카메라와 Gizmo 조작을 중단하되, 이미 클릭을 놓아 제출한 선택·배치 요청은 유지한다.
void FLevelEditorViewportClient::OnKeyboardFocusLost()
{
	bFocusAnimating = false;
	bTrackingBoxSelection = false;
	bBoxSelecting = false;
	bTrackingRightClick = false;
	TransformGizmo.OnKeyboardFocusLost();
	FEditorViewportClient::OnKeyboardFocusLost();
}

// 정상 확정 전에 캡처가 사라지면 Gizmo의 시작 Transform을 복원한다.
void FLevelEditorViewportClient::OnMouseCaptureLost()
{
	bFocusAnimating = false;
	bTrackingBoxSelection = false;
	bBoxSelecting = false;
	bTrackingRightClick = false;
	TransformGizmo.OnMouseCaptureLost();
	FEditorViewportClient::OnMouseCaptureLost();
}

// Viewport Panel이 보류된 Context Menu 배치 위치를 한 번 소비한다.
bool FLevelEditorViewportClient::ConsumeContextMenuRequest(FVector& OutPlacementLocation)
{
	if (!bContextMenuRequested)
	{
		return false;
	}
	OutPlacementLocation = ContextMenuPlacementLocation;
	bContextMenuRequested = false;
	return true;
}

// 진행 중인 박스 선택의 Application 좌표와 추가 선택 여부를 Viewport Panel에 제공한다.
bool FLevelEditorViewportClient::GetBoxSelection(FVector2& OutStart, FVector2& OutEnd, bool& bOutAdditive) const
{
	if (!bBoxSelecting)
	{
		return false;
	}
	OutStart = BoxSelectionStart;
	OutEnd = BoxSelectionEnd;
	bOutAdditive = HasModifierKey(BoxSelectionModifiers, EModifierKeyMask::Shift);
	return true;
}

// 기본 Scene View에 현재 선택의 View별 Gizmo Render 값을 복사한다.
FSceneView FLevelEditorViewportClient::BuildSceneView()
{
	FSceneView View = FEditorViewportClient::BuildSceneView();
	TransformGizmo.BuildGizmoView(Selection, View, View.Gizmo);
	return View;
}

// 선택 Node의 World Bounds가 화면에 들어오도록 시선 방향을 유지하며 카메라 위치와 직교 배율을 맞춘다.
void FLevelEditorViewportClient::FocusSelectedNode()
{
	UNode* Node = Selection.SelectedNode;
	if (!Node || !GetViewport().IsValid())
	{
		return;
	}

	FAABB Bounds;
	for (const TObjectPtr<UComponent>& Component : Node->GetComponents())
	{
		if (!Component->IsA(UStaticMeshComponent::StaticClass()))
		{
			continue;
		}
		const auto* MeshComponent = static_cast<const UStaticMeshComponent*>(Component.Get());
		const UStaticMesh* Mesh = MeshComponent->GetStaticMesh();
		if (MeshComponent->IsVisible() && Mesh && Mesh->GetMeshData()->GetLocalBounds().IsValid())
		{
			Bounds.Merge(Mesh->GetMeshData()->GetLocalBounds().Transform(MeshComponent->GetTransform().GetWorldMatrix()));
		}
	}

	static constexpr float FocusRadius = 0.5f;
	static constexpr float FocusMargin = 1.15f;
	const FVector Center = Bounds.IsValid() ? Bounds.GetCenter() : Node->GetTransform().GetWorldLocation();
	const float Radius = (Bounds.IsValid() ? std::max(Bounds.GetExtent().Size(), 0.01f) : FocusRadius) * FocusMargin;
	const FSceneView View = FEditorViewportClient::BuildSceneView();
	const FVector Forward = View.InverseViewMatrix.TransformVector(FVector(0.0f, 0.0f, 1.0f)).GetSafeNormal();
	const FEditorViewportCameraTransform& Transform = GetCamera().ViewTransform;
	FocusTargetOrthoZoom = Transform.OrthoZoom;
	float Distance = Radius + Transform.NearClip;
	if (Transform.bIsOrtho)
	{
		FocusTargetOrthoZoom = 2.0f * Radius * std::max(1.0f, Transform.AspectRatio);
	}
	else
	{
		// 가로·세로 중 좁은 시야각에 Bounding Sphere를 맞춰 세로로 긴 뷰포트에서도 잘리지 않게 한다.
		const float HalfVerticalFOV = KMath::ToRadian(std::clamp(Transform.FOV, 1.0f, 179.0f)) * 0.5f;
		const float HalfHorizontalFOV = std::atan(std::tan(HalfVerticalFOV) * Transform.AspectRatio);
		Distance = std::max(Distance, Radius / std::sin(std::min(HalfVerticalFOV, HalfHorizontalFOV)));
	}
	FocusStartLocation = Transform.ViewLocation;
	FocusTargetLocation = Center - Forward * Distance;
	FocusStartOrthoZoom = Transform.OrthoZoom;
	FocusElapsedTime = 0.0f;
	FocusTargetUUID = Node->GetUUID();
	LastFocusTransform = Transform;
	bFocusAnimating = true;
}

// 시작 상태에서 목표 상태까지 Smoothstep으로 보간하고 선택 변경이나 직접 카메라 조작 시 중단한다.
void FLevelEditorViewportClient::UpdateFocusAnimation(float DeltaTime)
{
	if (!bFocusAnimating)
	{
		return;
	}

	FEditorViewportCameraTransform& Transform = GetCamera().ViewTransform;
	if (!Selection.SelectedNode || Selection.SelectedNode->GetUUID() != FocusTargetUUID ||
	    IsCameraDragging() || TransformGizmo.IsDragging() ||
	    Transform.ViewLocation != LastFocusTransform.ViewLocation || Transform.ViewRotation != LastFocusTransform.ViewRotation ||
	    Transform.OrthoZoom != LastFocusTransform.OrthoZoom || Transform.FOV != LastFocusTransform.FOV || Transform.bIsOrtho != LastFocusTransform.bIsOrtho)
	{
		bFocusAnimating = false;
		return;
	}

	FocusElapsedTime = std::min(FocusElapsedTime + std::max(DeltaTime, 0.0f), FocusAnimationDuration);
	const float Progress = FocusElapsedTime / FocusAnimationDuration;
	const float Alpha = Progress * Progress * (3.0f - 2.0f * Progress);
	Transform.ViewLocation = FocusStartLocation + (FocusTargetLocation - FocusStartLocation) * Alpha;
	Transform.OrthoZoom = FocusStartOrthoZoom + (FocusTargetOrthoZoom - FocusStartOrthoZoom) * Alpha;
	if (FocusElapsedTime >= FocusAnimationDuration)
	{
		Transform.ViewLocation = FocusTargetLocation;
		Transform.OrthoZoom = FocusTargetOrthoZoom;
		bFocusAnimating = false;
	}
	LastFocusTransform = Transform;
}

// 현재 카메라와 클릭 위치로 Ray를 만들고 Ctrl/Shift 상태를 저장한 뒤 다음 렌더 프레임에서 RT가 검사하도록 요청한다.
void FLevelEditorViewportClient::RequestSelection(const FVector2& InputPosition, EModifierKeyMask Modifiers)
{
	FVector2 PixelPosition;
	UWorld* World = GetWorld();
	if (!World || !ContainsInputPosition(InputPosition) || !GetViewportPixelPosition(InputPosition, PixelPosition))
	{
		return;
	}

	const FSceneView View = FEditorViewportClient::BuildSceneView();
	const FRay Ray = FRay::BuildRay(PixelPosition.X, PixelPosition.Y, View.InverseViewProjectionMatrix, View.Viewport.Width, View.Viewport.Height);
	if (Ray.Direction.IsNearlyZero())
	{
		return;
	}

	FPendingRayQuery Pending;
	Pending.WorldUUID = World->GetUUID();
	Pending.Generation = Selection.GetGeneration();
	Pending.Modifiers = Modifiers;
	Pending.Query = World->GetScene().EnqueueRayQuery(Ray, View);
	PendingRayQueries.push_back(std::move(Pending));
}

// RT 검사가 끝난 요청부터 제출 순서대로 선택에 반영하고, World가 바뀌거나 다른 선택 동작이 있었다면 결과를 버린다.
void FLevelEditorViewportClient::ApplySelectionResult()
{
	while (!PendingRayQueries.empty())
	{
		const FPendingRayQuery& Front = PendingRayQueries.front();

		UWorld* World = GetWorld();
		if (!World || World->GetUUID() != Front.WorldUUID || Selection.GetGeneration() != Front.Generation)
		{
			PendingRayQueries.pop_front();
			continue;
		}

		if (!Front.Query->bReady.load(std::memory_order_acquire))
		{
			break;
		}

		const FPendingRayQuery Pending = Front;
		PendingRayQueries.pop_front();
		const FSceneRayQuery& Query = *Pending.Query;
		UPrimitiveComponent* Component = World->FindPrimitive(Query.PrimitiveId);

		if (Query.PrimitiveId != 0 && !Component)
		{
			continue;
		}

		UNode* HitNode = Component ? &Component->GetOwner() : nullptr;

		if (HasModifierKey(Pending.Modifiers, EModifierKeyMask::Control))
		{
			if (HitNode)
			{
				Selection.Toggle(*HitNode);
			}
		}
		else if (HasModifierKey(Pending.Modifiers, EModifierKeyMask::Shift))
		{
			if (HitNode)
			{
				Selection.Add(*HitNode);
			}
		}
		else
		{
			Selection.Select(HitNode);
		}

		// 앞선 클릭의 선택 적용으로 뒤따르는 Ctrl/Shift 클릭 결과까지 버리지 않도록, 같은 번호로 대기하던 요청의 번호를 갱신한다.
		for (FPendingRayQuery& Next : PendingRayQueries)
		{
			if (Next.Generation == Pending.Generation)
			{
				Next.Generation = Selection.GetGeneration();
			}
		}

	}
}

// 우클릭 Ray의 완료를 확인하고 메시 교차점 또는 입력 시점의 대체 위치로 Context Menu를 연다.
void FLevelEditorViewportClient::ApplyPlacementResult()
{
	if (!PendingPlacementQuery)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World || World->GetUUID() != PendingPlacementQuery->WorldUUID)
	{
		PendingPlacementQuery.reset();
		return;
	}
	if (!PendingPlacementQuery->Query->bReady.load(std::memory_order_acquire))
	{
		return;
	}

	const FPendingPlacementQuery Pending = std::move(*PendingPlacementQuery);
	PendingPlacementQuery.reset();
	const FSceneRayQuery& Query = *Pending.Query;
	if (Query.PrimitiveId != 0 && !World->FindPrimitive(Query.PrimitiveId))
	{
		return;
	}

	const bool bHit = Query.PrimitiveId != 0;
	ContextMenuPlacementLocation = bHit ? Query.HitPosition : Pending.FallbackPosition;
	bContextMenuRequested = true;
}

// Node의 World 원점을 화면에 투영하여 선택 사각형 안의 Node를 선택 집합에 반영한다.
void FLevelEditorViewportClient::ApplyBoxSelection()
{
	UWorld* World = GetWorld();
	FVector2 StartPixel;
	FVector2 EndPixel;
	if (!World || !GetViewportPixelPosition(BoxSelectionStart, StartPixel) || !GetViewportPixelPosition(BoxSelectionEnd, EndPixel))
	{
		return;
	}

	const float MinimumX = std::min(StartPixel.X, EndPixel.X);
	const float MinimumY = std::min(StartPixel.Y, EndPixel.Y);
	const float MaximumX = std::max(StartPixel.X, EndPixel.X);
	const float MaximumY = std::max(StartPixel.Y, EndPixel.Y);
	const FSceneView View = FEditorViewportClient::BuildSceneView();
	TArray<UNode*> NodesInBox;
	for (const TObjectPtr<ULevel>& Level : World->GetLevels())
	{
		for (const TObjectPtr<UNode>& Node : Level->GetNodes())
		{
			const FVector4 Clip = FVector4(Node->GetTransform().GetWorldLocation(), 1.0f) * View.ViewProjectionMatrix;
			if (Clip.W <= KMath::Epsilon)
			{
				continue;
			}

			const float InverseW = 1.0f / Clip.W;
			const float Depth = Clip.Z * InverseW;
			const float PixelX = (Clip.X * InverseW * 0.5f + 0.5f) * View.Viewport.Width;
			const float PixelY = (0.5f - Clip.Y * InverseW * 0.5f) * View.Viewport.Height;
			if (Depth >= 0.0f && Depth <= 1.0f && PixelX >= MinimumX && PixelX <= MaximumX && PixelY >= MinimumY && PixelY <= MaximumY)
			{
				NodesInBox.push_back(Node.Get());
			}
		}
	}

	if (HasModifierKey(BoxSelectionModifiers, EModifierKeyMask::Control))
	{
		for (UNode* Node : NodesInBox)
		{
			Selection.Toggle(*Node);
		}
	}
	else if (HasModifierKey(BoxSelectionModifiers, EModifierKeyMask::Shift))
	{
		for (UNode* Node : NodesInBox)
		{
			Selection.Add(*Node);
		}
	}
	else
	{
		Selection.Select(NodesInBox, NodesInBox.empty() ? nullptr : NodesInBox.back());
	}
}

// Ray가 메시를 맞히지 못했을 때 사용할 Z=0 평면 교차점 또는 카메라 전방 10m 위치를 클릭 시점에 계산한다.
FVector FLevelEditorViewportClient::FindPlacementLocation(const FRay& Ray)
{
	if (std::fabs(Ray.Direction.Z) > KMath::Epsilon)
	{
		const float Distance = -Ray.Origin.Z / Ray.Direction.Z;
		if (Distance > 0.0f)
		{
			return Ray.Origin + Ray.Direction * Distance;
		}
	}
	static constexpr float DefaultPlacementDistance = 10.0f;
	return Ray.Origin + Ray.Direction * DefaultPlacementDistance;
}

// 이전 우클릭 요청을 취소하고 새 Ray 검사를 요청하며, 검사 결과를 받은 뒤 배치 위치를 정해 Context Menu를 연다.
void FLevelEditorViewportClient::RequestContextMenu(const FVector2& InputPosition)
{
	PendingPlacementQuery.reset();
	bContextMenuRequested = false;
	FVector2 PixelPosition;
	UWorld* World = GetWorld();
	if (!World || !ContainsInputPosition(InputPosition) || !GetViewportPixelPosition(InputPosition, PixelPosition))
	{
		return;
	}

	const FSceneView View = FEditorViewportClient::BuildSceneView();
	const FRay Ray = FRay::BuildRay(PixelPosition.X, PixelPosition.Y, View.InverseViewProjectionMatrix, View.Viewport.Width, View.Viewport.Height);
	if (Ray.Direction.IsNearlyZero())
	{
		return;
	}

	FPendingPlacementQuery Pending;
	Pending.WorldUUID = World->GetUUID();
	Pending.FallbackPosition = FindPlacementLocation(Ray);
	Pending.Query = World->GetScene().EnqueueRayQuery(Ray, View);
	PendingPlacementQuery = std::move(Pending);
}
