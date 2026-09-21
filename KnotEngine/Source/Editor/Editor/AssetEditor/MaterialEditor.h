#pragma once

#include "Asset/Material/Material.h"
#include "Editor/AssetEditor/AssetEditor.h"

class UStaticMeshComponent;
class UWorld;

// Material Asset의 Shader, Pipeline 상태와 Parameter를 검사하고 기본 Geometry에 적용해 Preview하는 Document Panel이다.
class FMaterialEditor final : public FAssetEditor
{
public:
	FMaterialEditor(UEditorEngine& InEditorEngine, UMaterial& InMaterial);

protected:
	const char* GetEditorTypeName() const override { return "Material Editor"; }
	
	void CreatePreviewContent(UWorld& PreviewWorld) override;
	void DestroyPreviewContent() override;

	void DrawAssetToolbar() override;
	void DrawAssetDetails() override;

private:
	static const char* GetBlendModeName(EMaterialBlendMode BlendMode);
	static const char* GetDepthModeName(EDepthMode DepthMode);
	static const char* GetCullModeName(ECullMode CullMode);

	void ResetCamera();

	UMaterial& Material;
	UStaticMeshComponent* PreviewComponent = nullptr;
};
