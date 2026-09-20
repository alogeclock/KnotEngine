#include "Asset/Mesh/StaticMesh.h"

#include "Object/ReferenceCollector.h"

bool UStaticMesh::Initialize(
	const FAssetId& InAssetId,
	FString InAssetPath,
	FStaticMesh&& InRenderData,
	TArray<FStaticMaterial>&& InStaticMaterials)
{
	if (!InRenderData.IsValid() || !InitializeAsset(InAssetId, std::move(InAssetPath)))
	{
		return false;
	}

	RenderData = std::move(InRenderData);
	StaticMaterials = std::move(InStaticMaterials);
	Revision = 1;

	if (StaticMaterials.empty())
	{
		StaticMaterials.push_back({ FName("Default"), nullptr });
	}

	return true;
}

// UObject와 영속 AssetId는 유지한 채 재임포트된 CPU/GPU Mesh 데이터와 Material Slot을 교체한다.
bool UStaticMesh::Reload(FStaticMesh&& InRenderData, TArray<FStaticMaterial>&& InStaticMaterials)
{
	if (!InRenderData.IsValid())
	{
		return false;
	}

	RenderData = std::move(InRenderData);
	StaticMaterials = std::move(InStaticMaterials);
	if (StaticMaterials.empty())
	{
		StaticMaterials.push_back({ FName("Default"), nullptr });
	}
	++Revision;
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
