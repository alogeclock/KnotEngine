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

FInputReply FLevelEditorViewportClient::OnInputEvent(const FInputEvent& Event)
{
	const FPointerInputEvent* PointerEvent = std::get_if<FPointerInputEvent>(&Event);
	if (PointerEvent && PointerEvent->Type == EPointerInputEventType::ButtonDown && PointerEvent->Button == EMouseButton::Left)
	{
		Selection.Select(Raycast(PointerEvent->Position));
		return FInputReply::Handled().SetKeyboardFocus();
	}
	return FEditorViewportClient::OnInputEvent(Event);
}

// 입력 위치에서 만든 World Ray로 모든 LOD0 Triangle을 순회하여 가장 가까운 Node를 반환한다.
UNode* FLevelEditorViewportClient::Raycast(const FVector2& InputPosition)
{
	FVector2 PixelPosition;
	UWorld* World = GetWorld();
	if (!World || !GetViewportPixelPosition(InputPosition, PixelPosition))
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
