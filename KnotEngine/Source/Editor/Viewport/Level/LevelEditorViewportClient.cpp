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
	TransformGizmo.OnKeyboardFocusLost();
	FEditorViewportClient::OnKeyboardFocusLost();
}

// 정상 확정 전에 캡처가 사라지면 Gizmo의 시작 Transform을 복원한다.
void FLevelEditorViewportClient::OnMouseCaptureLost()
{
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
