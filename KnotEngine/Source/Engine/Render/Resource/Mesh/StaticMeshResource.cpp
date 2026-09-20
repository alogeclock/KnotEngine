#include "Render/Resource/Mesh/StaticMeshResource.h"

#include "Asset/Mesh/StaticMesh.h"
#include "Core/Assert.h"

bool FStaticMeshResource::Initialize(IRenderDevice& RenderDevice, const FStaticMesh& StaticMesh, uint64 Revision)
{
	Release();
	if (!StaticMesh.IsValid() || Revision == 0)
	{
		return false;
	}

	LODResources.reserve(StaticMesh.GetLODCount());
	for (const FStaticMeshLOD& LOD : StaticMesh.GetLODs())
	{
		const auto* VertexBytes = reinterpret_cast<const uint8*>(LOD.GetVertices().data());
		const FMeshDataView DataView = {
			std::span<const uint8>(VertexBytes, LOD.GetVertices().size() * sizeof(FStaticMeshVertex)),
			LOD.GetIndices(),
			&FStaticMeshVertex::GetVertexLayout(),
			static_cast<uint32>(LOD.GetVertices().size())
		};
		FStaticMeshLODResource& LODResource = LODResources.emplace_back();
		if (!LODResource.MeshBuffer.Initialize(RenderDevice, DataView))
		{
			Release();
			return false;
		}
		LODResource.Sections.reserve(LOD.GetSections().size());
		for (const FStaticMeshSection& Section : LOD.GetSections())
		{
			LODResource.Sections.push_back({ Section.FirstIndex, Section.IndexCount, Section.MaterialIndex });
		}
	}
	SourceRevision = Revision;
	return true;
}

void FStaticMeshResource::Release()
{
	LODResources.clear();
	SourceRevision = 0;
}

const FStaticMeshLODResource& FStaticMeshResource::GetLOD(SIZE_T LODIndex) const
{
	check(LODIndex < LODResources.size());
	return LODResources[LODIndex];
}

bool FStaticMeshResource::IsValid() const
{
	if (SourceRevision == 0 || LODResources.empty())
	{
		return false;
	}
	for (const FStaticMeshLODResource& LODResource : LODResources)
	{
		if (!LODResource.IsValid())
		{
			return false;
		}
	}
	return true;
}
