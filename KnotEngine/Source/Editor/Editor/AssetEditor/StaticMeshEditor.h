#pragma once

#include "Editor/AssetEditor/AssetEditor.h"

class UStaticMesh;
class UStaticMeshComponent;
class UWorld;

// Static Mesh Asset의 LOD, Section, Material Slot을 검사하고 실제 Scene 경로로 Preview하는 Document Panel이다.
class FStaticMeshEditor final : public FAssetEditor
{
public:
	FStaticMeshEditor(UEditorEngine& InEditorEngine, UStaticMesh& InStaticMesh);

protected:
	const char* GetEditorTypeName() const override { return "Static Mesh Editor"; }
	
	void CreatePreviewContent(UWorld& PreviewWorld) override;
	void DestroyPreviewContent() override;
	
	void DrawAssetToolbar() override;
	void DrawAssetDetails() override;

private:
	void ResetCamera();

	UStaticMesh& StaticMesh;
	UStaticMeshComponent* PreviewComponent = nullptr;
	SIZE_T SelectedLOD = 0;
};
