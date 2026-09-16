#include "Render/Resource/Mesh.h"

#include "Core/Assert.h"

#include <limits>

// C 배열과 TArray 등 연속 메모리를 소유권 이전 없이 크기와 함께 받기 위해 span을 사용한다.
// 입력 데이터는 함수 안에서 CPU Mesh 배열로 복사하므로 span은 호출 중에만 유효하면 된다.
void FGeometryMesh::Initialize(std::span<const FGeometryVertex> InVertices, std::span<const uint32> InIndices)
{
	Release();

	Vertices.assign(InVertices.begin(), InVertices.end());
	Indices.assign(InIndices.begin(), InIndices.end());
	LocalBounds.Reset();
	for (const FGeometryVertex& Vertex : Vertices)
	{
		LocalBounds.Expand(Vertex.Position);
	}
}

bool FGeometryMesh::InitResources(IRenderDevice& RenderDevice)
{
	if (MeshBuffer.IsValid())
	{
		return true;
	}

	checkf(!Vertices.empty() && Vertices.size() <= (std::numeric_limits<uint32>::max)(),
		"Geometry Mesh의 Vertex 데이터가 유효하지 않다. VertexCount={}", Vertices.size());
	const auto* VertexBytes = reinterpret_cast<const uint8*>(Vertices.data());
	const FMeshDataView DataView = {
		std::span<const uint8>(VertexBytes, Vertices.size() * sizeof(FGeometryVertex)),
		std::span<const uint32>(Indices.data(), Indices.size()),
		&FGeometryVertex::GetVertexLayout(),
		static_cast<uint32>(Vertices.size())
	};
	return MeshBuffer.Initialize(RenderDevice, DataView);
}

// FGeometryMesh는 남긴 채, GPU에 업로드된 데이터를 해제한다.
void FGeometryMesh::Release()
{
	MeshBuffer.Release();
}

bool FStaticMeshLOD::Initialize(std::span<const FStaticMeshVertex> InVertices, std::span<const uint32> InIndices)
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

	Release();
	Vertices.assign(InVertices.begin(), InVertices.end());
	Indices.assign(InIndices.begin(), InIndices.end());
	LocalBounds.Reset();
	for (const FStaticMeshVertex& Vertex : Vertices)
	{
		LocalBounds.Expand(Vertex.Position);
	}
	return true;
}

bool FStaticMeshLOD::InitResources(IRenderDevice& RenderDevice)
{
	if (MeshBuffer.IsValid())
	{
		return true;
	}
	if (!IsValid())
	{
		return false;
	}

	const auto* VertexBytes = reinterpret_cast<const uint8*>(Vertices.data());
	const FMeshDataView DataView = {
		std::span<const uint8>(VertexBytes, Vertices.size() * sizeof(FStaticMeshVertex)),
		std::span<const uint32>(Indices.data(), Indices.size()),
		&FStaticMeshVertex::GetVertexLayout(),
		static_cast<uint32>(Vertices.size())
	};
	return MeshBuffer.Initialize(RenderDevice, DataView);
}

void FStaticMeshLOD::Release()
{
	MeshBuffer.Release();
}

bool FStaticMesh::AddLOD(std::span<const FStaticMeshVertex> Vertices, std::span<const uint32> Indices)
{
	FStaticMeshLOD LOD;
	if (!LOD.Initialize(Vertices, Indices))
	{
		return false;
	}

	LocalBounds.Merge(LOD.GetLocalBounds());
	LODs.push_back(std::move(LOD));
	return true;
}

bool FStaticMesh::InitResources(IRenderDevice& RenderDevice)
{
	if (!IsValid())
	{
		return false;
	}

	for (FStaticMeshLOD& LOD : LODs)
	{
		if (!LOD.InitResources(RenderDevice))
		{
			Release();
			return false;
		}
	}
	return true;
}

void FStaticMesh::Release()
{
	for (FStaticMeshLOD& LOD : LODs)
	{
		LOD.Release();
	}
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
