#include "LevelEditorViewportClient.h"

#include "Core/Geometry/Ray.h"
#include "Core/Math/Matrix.h"
#include "Editor/EditorSelection.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Render/Renderer.h"
#include "Render/Resource/Mesh/Mesh.h"
#include "Render/Scene/Scene.h"
#include "Viewport/Viewport.h"
#include "World/World.h"

#include <cmath>
#include <limits>
#include <variant>

FLevelEditorViewportClient::FLevelEditorViewportClient(FViewport& InViewport, FEditorSelection& InSelection)
	: FEditorViewportClient(InViewport), Selection(InSelection)
{
}

FSceneView FLevelEditorViewportClient::BuildSceneView()
{
	FSceneView View = FEditorViewportClient::BuildSceneView();
	View.SelectedNode = Selection.SelectedNode;
	return View;
}

FInputReply FLevelEditorViewportClient::OnInputEvent(const FInputEvent& Event)
{
	const FPointerInputEvent* PointerEvent = std::get_if<FPointerInputEvent>(&Event);
	if (PointerEvent && PointerEvent->Type == EPointerInputEventType::ButtonDown && PointerEvent->Button == EMouseButton::Left)
	{
		Selection.SelectedNode = Raycast(PointerEvent->Position);
		return FInputReply::Handled().SetKeyboardFocus();
	}
	return FEditorViewportClient::OnInputEvent(Event);
}

// 입력 위치에서 만든 World Ray로 모든 LOD0 Triangle을 순회하여 가장 가까운 Node를 반환한다.
UNode* FLevelEditorViewportClient::Raycast(const FVector2& InputPosition)
{
	FVector2 PixelPosition;
	FScene* Scene = GetScene();
	if (!Scene || !GetViewportPixelPosition(InputPosition, PixelPosition))
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
	for (const std::unique_ptr<FPrimitiveSceneProxy>& Entry : Scene->GetProxies())
	{
		FPrimitiveSceneProxy& Primitive = *Entry;
		Primitive.Update();
		if (!Primitive.bVisible)
		{
			continue;
		}

		float Near = 0.0f;
		float Far = 0.0f;
		if (!Primitive.WorldBounds.IntersectRay(WorldRay, Near, Far) || Near > ClosestDistance)
		{
			continue;
		}

		const auto& StaticMeshProxy = static_cast<const FStaticMeshSceneProxy&>(Primitive);
		if (!StaticMeshProxy.Mesh || StaticMeshProxy.Mesh->GetLODCount() == 0 || std::fabs(Primitive.WorldMatrix.GetDeterminant()) <= KMath::Epsilon)
		{
			continue;
		}

		const FMatrix InverseWorld = Primitive.WorldMatrix.GetInverse();
		const FRay LocalRay(InverseWorld.TransformPosition(WorldRay.Origin), InverseWorld.TransformVector(WorldRay.Direction));
		const FStaticMeshLOD& LOD = StaticMeshProxy.Mesh->GetLOD(0);
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
				ClosestNode = &Primitive.GetOwner();
			}
		}
	}
	return ClosestNode;
}
