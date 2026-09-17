#include "Asset/Mesh/StaticMesh.h"

#include "Object/ReferenceCollector.h"

bool UStaticMesh::Initialize(FString InAssetPath, FStaticMesh&& InRenderData, TArray<FStaticMaterial>&& InStaticMaterials)
{
	if (InAssetPath.empty() || !InRenderData.IsValid())
	{
		return false;
	}

	AssetPath = std::move(InAssetPath);
	RenderData = std::move(InRenderData);
	StaticMaterials = std::move(InStaticMaterials);

	if (StaticMaterials.empty())
	{
		StaticMaterials.push_back({ FName("Default"), nullptr });
	}

	return true;
}

UMaterialInterface* UStaticMesh::GetMaterial(SIZE_T MaterialIndex) const
{
	return MaterialIndex < StaticMaterials.size() ? StaticMaterials[MaterialIndex].Material.Get() : nullptr;
}

void UStaticMesh::AddReferencedObjects(FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
	for (const FStaticMaterial& StaticMaterial : StaticMaterials)
	{
		Collector.AddReferencedObject(StaticMaterial.Material);
	}
}
