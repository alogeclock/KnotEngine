#include "Render/Resource/Mesh/StaticMeshResource.h"

#include "Asset/Mesh/StaticMesh.h"
#include "Core/Assert.h"

#include <utility>

// Command의 CPU Mesh로 GPU Buffer를 생성하고 성공한 Mesh의 소유권을 이동받아 RT 삼각형 검사에도 사용한다.
bool FStaticMeshResource::Initialize(IRenderDevice& RenderDevice, FStaticMesh&& StaticMesh, uint64 Revision)
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

	MeshData = std::move(StaticMesh);
	SourceRevision = Revision;
	return true;
}

// RT가 소유한 GPU Buffer와 CPU Mesh 데이터를 함께 해제하고 적용 Revision을 초기화한다.
void FStaticMeshResource::Release()
{
	LODResources.clear();
	MeshData = FStaticMesh();
	SourceRevision = 0;
}

const FStaticMeshLODResource& FStaticMeshResource::GetLOD(SIZE_T LODIndex) const
{
	check(LODIndex < LODResources.size());
	return LODResources[LODIndex];
}

bool FStaticMeshResource::IsValid() const
{
	if (SourceRevision == 0 || !MeshData.IsValid() || LODResources.empty() || MeshData.GetLODCount() != LODResources.size())
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
