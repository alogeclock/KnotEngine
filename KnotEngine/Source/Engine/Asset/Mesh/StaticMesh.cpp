#include "Asset/Mesh/StaticMesh.h"

#include "Core/Assert.h"
#include "Object/ReferenceCollector.h"

#include <limits>

bool FStaticMeshLOD::Initialize(
	std::span<const FStaticMeshVertex> InVertices,
	std::span<const uint32> InIndices,
	std::span<const FStaticMeshSection> InSections)
{
	if (InVertices.empty() || InVertices.size() > (std::numeric_limits<uint32>::max)() ||
		InIndices.size() > (std::numeric_limits<uint32>::max)())
	{
		return false;
	}
	for (uint32 Index : InIndices)
	{
		if (Index >= InVertices.size())
		{
			return false;
		}
	}
	const uint32 ElementCount = static_cast<uint32>(InIndices.empty() ? InVertices.size() : InIndices.size());
	for (const FStaticMeshSection& Section : InSections)
	{
		if (Section.IndexCount == 0 || Section.FirstIndex > ElementCount || Section.IndexCount > ElementCount - Section.FirstIndex)
		{
			return false;
		}
	}

	Vertices.assign(InVertices.begin(), InVertices.end());
	Indices.assign(InIndices.begin(), InIndices.end());
	Sections.assign(InSections.begin(), InSections.end());
	if (Sections.empty())
	{
		Sections.push_back({ 0, ElementCount, 0 });
	}
	LocalBounds.Reset();
	for (const FStaticMeshVertex& Vertex : Vertices)
	{
		LocalBounds.Expand(Vertex.Position);
	}
	return true;
}

bool FStaticMesh::AddLOD(std::span<const FStaticMeshVertex> Vertices, std::span<const uint32> Indices, std::span<const FStaticMeshSection> Sections)
{
	FStaticMeshLOD LOD;
	if (!LOD.Initialize(Vertices, Indices, Sections))
	{
		return false;
	}

	LocalBounds.Merge(LOD.GetLocalBounds());
	LODs.push_back(std::move(LOD));
	return true;
}

FStaticMeshLOD& FStaticMesh::GetLOD(SIZE_T LODIndex)
{
	check(LODIndex < LODs.size());
	return LODs[LODIndex];
}

const FStaticMeshLOD& FStaticMesh::GetLOD(SIZE_T LODIndex) const
{
	check(LODIndex < LODs.size());
	return LODs[LODIndex];
}

bool UStaticMesh::Initialize(
	const FAssetId& InAssetId,
	FString InAssetPath,
	FStaticMesh&& InMeshData,
	TArray<FStaticMaterial>&& InStaticMaterials)
{
	if (!InMeshData.IsValid() || !InitializeAsset(InAssetId, std::move(InAssetPath)))
	{
		return false;
	}

	MeshData = std::move(InMeshData);
	StaticMaterials = std::move(InStaticMaterials);
	Revision = 1;

	if (StaticMaterials.empty())
	{
		StaticMaterials.push_back({ FName("Default"), nullptr });
	}

	return true;
}

// UObject와 영속 AssetId는 유지한 채 재임포트된 CPU Mesh 데이터와 Material Slot을 교체한다.
bool UStaticMesh::Reload(FStaticMesh&& InMeshData, TArray<FStaticMaterial>&& InStaticMaterials)
{
	if (!InMeshData.IsValid())
	{
		return false;
	}

	MeshData = std::move(InMeshData);
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
