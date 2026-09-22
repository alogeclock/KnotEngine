#include "LevelEditorViewportClient.h"

#include "Core/Geometry/Ray.h"
#include "Core/Math/Matrix.h"
#include "Component/Mesh/StaticMeshComponent.h"
#include "Component/TransformComponent.h"
#include "Editor/EditorSelection.h"
#include "Viewport/Viewport.h"
#include "World/World.h"
#include "World/Level.h"
#include "World/Node.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <variant>

FLevelEditorViewportClient::FLevelEditorViewportClient(FViewport& InViewport, FEditorSelection& InSelection)
	: FEditorViewportClient(InViewport), Selection(InSelection)
{
}

// 입력 이벤트가 없는 프레임에도 선택 변경과 대상 제거를 반영하고 일반 Viewport Tick을 수행한다.
void FLevelEditorViewportClient::Tick(float DeltaTime)
{
	TransformGizmo.UpdateSelection(Selection);
	FEditorViewportClient::Tick(DeltaTime);
	UpdateFocusAnimation(DeltaTime);
}

// 진행 중인 카메라 드래그를 우선 처리하고 일반 왼쪽 클릭은 장면 선택으로 전달한다.
FInputReply FLevelEditorViewportClient::OnInputEvent(const FInputEvent& Event)
{
	TransformGizmo.UpdateSelection(Selection);

	if (IsCameraDragging())
	{
		return FEditorViewportClient::OnInputEvent(Event);
	}

	const FPointerInputEvent* PointerEvent = std::get_if<FPointerInputEvent>(&Event);
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
		return GizmoReply;
	}

	if (const FKeyInputEvent* KeyEvent = std::get_if<FKeyInputEvent>(&Event))
	{
		if (KeyEvent->Key == EKeyboardKey::Escape && KeyEvent->bDown && bFocusAnimating)
		{
			bFocusAnimating = false;
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
		Selection.Select(Raycast(PointerEvent->Position));
		return FInputReply::Handled().SetKeyboardFocus();
	}

	return FEditorViewportClient::OnInputEvent(Event);
}

// 키보드 포커스 상실을 카메라와 진행 중인 Gizmo 조작에 함께 전달한다.
void FLevelEditorViewportClient::OnKeyboardFocusLost()
{
	bFocusAnimating = false;
	TransformGizmo.OnKeyboardFocusLost();
	FEditorViewportClient::OnKeyboardFocusLost();
}

// 정상 확정 전에 캡처가 사라지면 Gizmo의 시작 Transform을 복원한다.
void FLevelEditorViewportClient::OnMouseCaptureLost()
{
	bFocusAnimating = false;
	TransformGizmo.OnMouseCaptureLost();
	FEditorViewportClient::OnMouseCaptureLost();
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
		if (MeshComponent->IsVisible() && Mesh && Mesh->GetMeshData().GetLocalBounds().IsValid())
		{
			Bounds.Merge(Mesh->GetMeshData().GetLocalBounds().Transform(MeshComponent->GetTransform().GetWorldMatrix()));
		}
	}

	static constexpr float FocusRadius = 50.0f;
	static constexpr float FocusMargin = 1.15f;
	const FVector Center = Bounds.IsValid() ? Bounds.GetCenter() : Node->GetTransform().GetWorldLocation();
	const float Radius = (Bounds.IsValid() ? std::max(Bounds.GetExtent().Size(), 1.0f) : FocusRadius) * FocusMargin;
	const FSceneView View = FEditorViewportClient::BuildSceneView();
	const FVector Forward = View.ViewMatrix.GetInverse().TransformVector(FVector(0.0f, 0.0f, 1.0f)).GetSafeNormal();
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

// 입력 위치에서 만든 World Ray로 모든 LOD0 Triangle을 순회하여 가장 가까운 Node를 반환한다.
UNode* FLevelEditorViewportClient::Raycast(const FVector2& InputPosition)
{
	FVector2 PixelPosition;
	UWorld* World = GetWorld();
	if (!World || !ContainsInputPosition(InputPosition) || !GetViewportPixelPosition(InputPosition, PixelPosition))
	{
		return nullptr;
	}

	const FSceneView View = FEditorViewportClient::BuildSceneView();
	const FRay WorldRay = FRay::BuildRay(PixelPosition.X, PixelPosition.Y, View.ViewProjectionMatrix, View.Viewport.Width, View.Viewport.Height);
	if (WorldRay.Direction.IsNearlyZero())
	{
		return nullptr;
	}

	UNode* ClosestNode = nullptr;
	float ClosestDistance = (std::numeric_limits<float>::max)();
	for (const TObjectPtr<ULevel>& Level : World->GetLevels())
	{
		for (const TObjectPtr<UNode>& Node : Level->GetNodes())
		{
			for (const TObjectPtr<UComponent>& Component : Node->GetComponents())
			{
				if (!Component->IsA(UStaticMeshComponent::StaticClass()))
				{
					continue;
				}
				auto* StaticMeshComponent = static_cast<UStaticMeshComponent*>(Component.Get());
				UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
				if (!StaticMeshComponent->IsVisible() || !StaticMesh || StaticMesh->GetMeshData().GetLODCount() == 0)
				{
					continue;
				}
				const FMatrix WorldMatrix = StaticMeshComponent->GetTransform().GetWorldMatrix();
				if (std::fabs(WorldMatrix.GetDeterminant()) <= KMath::Epsilon)
				{
					continue;
				}
				const FAABB WorldBounds = StaticMesh->GetMeshData().GetLocalBounds().Transform(WorldMatrix);
				float Near = 0.0f;
				float Far = 0.0f;
				if (!WorldBounds.IntersectRay(WorldRay, Near, Far) || Near > ClosestDistance)
				{
					continue;
				}
				const FMatrix InverseWorld = WorldMatrix.GetInverse();
				const FRay LocalRay(InverseWorld.TransformPosition(WorldRay.Origin), InverseWorld.TransformVector(WorldRay.Direction));
				const FStaticMeshLOD& LOD = StaticMesh->GetMeshData().GetLOD(0);
				const TArray<FStaticMeshVertex>& Vertices = LOD.GetVertices();
				const TArray<uint32>& Indices = LOD.GetIndices();
				const SIZE_T ElementCount = Indices.empty() ? Vertices.size() : Indices.size();
				for (SIZE_T ElementIndex = 0; ElementIndex + 2 < ElementCount; ElementIndex += 3)
				{
					const uint32 Index0 = Indices.empty() ? static_cast<uint32>(ElementIndex) : Indices[ElementIndex];
					const uint32 Index1 = Indices.empty() ? static_cast<uint32>(ElementIndex + 1) : Indices[ElementIndex + 1];
					const uint32 Index2 = Indices.empty() ? static_cast<uint32>(ElementIndex + 2) : Indices[ElementIndex + 2];
					if (Index0 >= Vertices.size() || Index1 >= Vertices.size() || Index2 >= Vertices.size())
					{
						continue;
					}
					float Distance = 0.0f;
					if (LocalRay.Intersect(Vertices[Index0].Position, Vertices[Index1].Position, Vertices[Index2].Position, Distance) && Distance < ClosestDistance)
					{
						ClosestDistance = Distance;
						ClosestNode = Node.Get();
					}
				}
			}
		}
	}
	return ClosestNode;
}
