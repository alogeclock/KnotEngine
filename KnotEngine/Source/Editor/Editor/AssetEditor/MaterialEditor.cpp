#include "Editor/AssetEditor/MaterialEditor.h"

#include "Asset/Asset/EngineAssetIds.h"
#include "Asset/AssetManager.h"
#include "Asset/Texture/Texture.h"
#include "Component/Mesh/StaticMeshComponent.h"
#include "World/Level.h"
#include "World/Node.h"
#include "World/World.h"

#include <imgui.h>

FMaterialEditor::FMaterialEditor(UEditorEngine& InEditorEngine, UMaterial& InMaterial)
	: FAssetEditor(InEditorEngine, InMaterial), Material(InMaterial)
{
}

// Preview World에 기본 Geometry를 만들고 편집 대상 Material을 첫 번째 Slot Override로 적용한다.
void FMaterialEditor::CreatePreviewContent(UWorld& PreviewWorld)
{
	check(GAssetManager);
	UStaticMesh* PreviewStaticMesh = GAssetManager->LoadStaticMesh(FEngineAssetIds::Sphere);
	panicf(PreviewStaticMesh, "Material Preview Sphere를 불러오지 못했다. AssetId={}", FEngineAssetIds::Sphere.ToString());

	UNode& PreviewNode = PreviewWorld.GetPersistentLevel().CreateNode(FName("MaterialPreview"));
	PreviewComponent = &PreviewNode.AddComponent<UStaticMeshComponent>();
	PreviewComponent->SetStaticMesh(PreviewStaticMesh);
	PreviewComponent->SetMaterial(0, &Material);
	ResetCamera();
}

void FMaterialEditor::DestroyPreviewContent()
{
	PreviewComponent = nullptr;
}

void FMaterialEditor::DrawAssetToolbar()
{
	if (ImGui::Button("Reset Camera"))
	{
		ResetCamera();
	}
}

// Material의 Shader Key, 고정 Pipeline 상태와 현재 기본 Parameter 값을 표시한다.
void FMaterialEditor::DrawAssetDetails()
{
	ImGui::TextUnformatted("Asset");
	ImGui::Separator();
	ImGui::TextWrapped("Path: %s", Material.GetAssetPath().c_str());
	ImGui::TextWrapped("ID: %s", Material.GetAssetId().ToString().c_str());

	const FMaterial* RenderMaterial = Material.GetMaterial();
	check(RenderMaterial);
	ImGui::Spacing();
	ImGui::TextUnformatted("Shaders");
	ImGui::Separator();
	const FShaderKey& VertexShader = RenderMaterial->GetVertexShader();
	const FShaderKey& PixelShader = RenderMaterial->GetPixelShader();
	ImGui::TextWrapped("VS: %s", VertexShader.SourcePath.c_str());
	ImGui::TextDisabled("Entry: %s  Permutation: %u", VertexShader.EntryPoint.c_str(), VertexShader.PermutationId);
	ImGui::TextWrapped("PS: %s", PixelShader.SourcePath.c_str());
	ImGui::TextDisabled("Entry: %s  Permutation: %u", PixelShader.EntryPoint.c_str(), PixelShader.PermutationId);

	ImGui::Spacing();
	ImGui::TextUnformatted("Pipeline");
	ImGui::Separator();
	ImGui::Text("Blend: %s", GetBlendModeName(RenderMaterial->GetBlendMode()));
	ImGui::Text("Depth: %s", GetDepthModeName(RenderMaterial->GetDepthMode()));
	ImGui::Text("Cull: %s", GetCullModeName(RenderMaterial->GetCullMode()));

	ImGui::Spacing();
	ImGui::Text("Scalar Parameters (%zu)", Material.GetScalarParameters().size());
	ImGui::Separator();
	for (const FScalarMaterialParameter& Parameter : Material.GetScalarParameters())
	{
		ImGui::Text("%s", Parameter.Name.ToString().c_str());
		ImGui::SameLine();
		ImGui::TextDisabled("%.4f", Parameter.Value);
	}

	ImGui::Spacing();
	ImGui::Text("Vector Parameters (%zu)", Material.GetVectorParameters().size());
	ImGui::Separator();
	for (const FVectorMaterialParameter& Parameter : Material.GetVectorParameters())
	{
		ImGui::Text("%s", Parameter.Name.ToString().c_str());
		ImGui::TextDisabled("   %.3f, %.3f, %.3f, %.3f", Parameter.Value.X, Parameter.Value.Y, Parameter.Value.Z, Parameter.Value.W);
	}

	ImGui::Spacing();
	ImGui::Text("Texture Parameters (%zu)", Material.GetTextureParameters().size());
	ImGui::Separator();
	for (const FTextureMaterialParameter& Parameter : Material.GetTextureParameters())
	{
		ImGui::Text("%s", Parameter.Name.ToString().c_str());
		ImGui::TextDisabled("   %s", Parameter.Texture ? Parameter.Texture->GetAssetPath().c_str() : "None");
	}
}

const char* FMaterialEditor::GetBlendModeName(EMaterialBlendMode BlendMode)
{
	switch (BlendMode)
	{
	case EMaterialBlendMode::Opaque: return "Opaque";
	case EMaterialBlendMode::Masked: return "Masked";
	case EMaterialBlendMode::Translucent: return "Translucent";
	default: return "Unknown";
	}
}

const char* FMaterialEditor::GetDepthModeName(EDepthMode DepthMode)
{
	switch (DepthMode)
	{
	case EDepthMode::ReadWrite: return "Read Write";
	case EDepthMode::ReadOnly: return "Read Only";
	case EDepthMode::Disabled: return "Disabled";
	default: return "Unknown";
	}
}

const char* FMaterialEditor::GetCullModeName(ECullMode CullMode)
{
	switch (CullMode)
	{
	case ECullMode::None: return "None";
	case ECullMode::Front: return "Front";
	case ECullMode::Back: return "Back";
	default: return "Unknown";
	}
}

void FMaterialEditor::ResetCamera()
{
	FEditorViewportCameraTransform& ViewTransform = GetAssetViewportClient().GetCamera().ViewTransform;
	ViewTransform.ViewLocation = FVector(-180.0f, 180.0f, 140.0f);
	ViewTransform.LookAt(FVector(0.0f, 0.0f, 50.0f));
}
