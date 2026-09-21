#include "Editor/AssetEditor/StaticMeshEditor.h"

#include "Asset/Material/MaterialInterface.h"
#include "Asset/Mesh/StaticMesh.h"
#include "Component/Mesh/StaticMeshComponent.h"
#include "Core/Geometry/AABB.h"
#include "World/Level.h"
#include "World/Node.h"
#include "World/World.h"

#include <algorithm>
#include <imgui.h>

FStaticMeshEditor::FStaticMeshEditor(
	UEditorEngine& InEditorEngine,
	FRenderSystem& InRenderSystem,
	FInputRouter& InInputRouter,
	FViewportToolbar& InViewportToolbar,
	UStaticMesh& InStaticMesh)
	: FAssetEditor(InEditorEngine, InRenderSystem, InInputRouter, InViewportToolbar, InStaticMesh), StaticMesh(InStaticMesh)
{
}

// Preview World에 대상 Static Mesh를 사용하는 단일 Component를 만든다.
void FStaticMeshEditor::CreatePreviewContent(UWorld& PreviewWorld)
{
	UNode& PreviewNode = PreviewWorld.GetPersistentLevel().CreateNode(FName("StaticMeshPreview"));
	PreviewComponent = &PreviewNode.AddComponent<UStaticMeshComponent>();
	PreviewComponent->SetStaticMesh(&StaticMesh);
	GetAssetViewportClient().GetForcedLODIndex() = static_cast<int32>(SelectedLOD);
	ResetCamera();
}

void FStaticMeshEditor::DestroyPreviewContent()
{
	PreviewComponent = nullptr;
}

void FStaticMeshEditor::DrawAssetToolbar()
{
	if (ImGui::Button("Reset Camera"))
	{
		ResetCamera();
	}
}

// Asset 식별 정보와 LOD별 Vertex, Index, Section 및 Material Slot 정보를 표시한다.
void FStaticMeshEditor::DrawAssetDetails()
{
	ImGui::TextUnformatted("Asset");
	ImGui::Separator();
	ImGui::TextWrapped("Path: %s", StaticMesh.GetAssetPath().c_str());
	ImGui::TextWrapped("ID: %s", StaticMesh.GetAssetId().ToString().c_str());

	const FStaticMesh& MeshData = StaticMesh.GetMeshData();
	const FAABB& Bounds = MeshData.GetLocalBounds();
	ImGui::Spacing();
	ImGui::TextUnformatted("Bounds");
	ImGui::Separator();
	ImGui::Text("Min: %.2f, %.2f, %.2f", Bounds.Min.X, Bounds.Min.Y, Bounds.Min.Z);
	ImGui::Text("Max: %.2f, %.2f, %.2f", Bounds.Max.X, Bounds.Max.Y, Bounds.Max.Z);

	ImGui::Spacing();
	ImGui::Text("LODs (%zu)", MeshData.GetLODCount());
	ImGui::Separator();
	if (MeshData.GetLODCount() > 0)
	{
		SelectedLOD = std::min(SelectedLOD, MeshData.GetLODCount() - 1);
		GetAssetViewportClient().GetForcedLODIndex() = static_cast<int32>(SelectedLOD);
		int32 LODIndex = static_cast<int32>(SelectedLOD);
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::SliderInt("##SelectedLOD", &LODIndex, 0, static_cast<int32>(MeshData.GetLODCount() - 1), "LOD %d"))
		{
			SelectedLOD = static_cast<SIZE_T>(LODIndex);
			GetAssetViewportClient().GetForcedLODIndex() = LODIndex;
		}
		const FStaticMeshLOD& LOD = MeshData.GetLOD(SelectedLOD);
		ImGui::Text("Vertices: %zu", LOD.GetVertices().size());
		ImGui::Text("Indices: %zu", LOD.GetIndices().size());
		ImGui::Text("Triangles: %zu", LOD.GetIndices().size() / 3);
		ImGui::Text("Sections: %zu", LOD.GetSections().size());
		for (SIZE_T SectionIndex = 0; SectionIndex < LOD.GetSections().size(); ++SectionIndex)
		{
			const FStaticMeshSection& Section = LOD.GetSections()[SectionIndex];
			ImGui::BulletText(
				"Section %zu: First %u, Count %u, Material %u",
				SectionIndex,
				Section.FirstIndex,
				Section.IndexCount,
				Section.MaterialIndex);
		}
	}

	ImGui::Spacing();
	ImGui::Text("Materials (%zu)", StaticMesh.GetMaterialCount());
	ImGui::Separator();
	const TArray<FStaticMaterial>& Materials = StaticMesh.GetStaticMaterials();
	for (SIZE_T MaterialIndex = 0; MaterialIndex < Materials.size(); ++MaterialIndex)
	{
		const FStaticMaterial& Slot = Materials[MaterialIndex];
		const UMaterialInterface* Material = Slot.Material.Get();
		ImGui::Text("%zu. %s", MaterialIndex, Slot.SlotName.ToString().c_str());
		ImGui::TextDisabled("   %s", Material ? Material->GetAssetPath().c_str() : "Default Material");
	}
}

void FStaticMeshEditor::ResetCamera()
{
	const FAABB& Bounds = StaticMesh.GetMeshData().GetLocalBounds();
	const FVector Center = Bounds.GetCenter();
	const FVector Extent = Bounds.GetExtent();
	const float Radius = (std::max)({ Extent.X, Extent.Y, Extent.Z, 25.0f });
	FEditorViewportCameraTransform& ViewTransform = GetAssetViewportClient().GetCamera().ViewTransform;
	ViewTransform.ViewLocation = Center + FVector(-Radius * 3.0f, Radius * 3.0f, Radius * 2.0f);
	ViewTransform.LookAt(Center);
}
