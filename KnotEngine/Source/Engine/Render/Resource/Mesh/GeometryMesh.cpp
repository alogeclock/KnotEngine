#include "Render/Resource/Mesh/GeometryMesh.h"

#include "Core/Assert.h"

#include <limits>

// 입력 배열은 CPU Geometry에 복사하므로 span은 호출 중에만 유효하면 된다.
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

// CPU Geometry는 유지하고 GPU Buffer만 해제한다.
void FGeometryMesh::Release()
{
	MeshBuffer.Release();
}
