#pragma once

#include "EngineAPI.h"

#include "Render/Resource/VertexTypes.h"

#include <span>

class FMeshBuffer;
class URenderer;

// 단순 Geometry Mesh의 CPU 원본 데이터와 GPU Mesh Buffer를 소유한다.
class ENGINE_API FGeometryMesh
{
public:
	FGeometryMesh();
	~FGeometryMesh();

	FGeometryMesh(const FGeometryMesh&) = delete;
	FGeometryMesh& operator=(const FGeometryMesh&) = delete;
	FGeometryMesh(FGeometryMesh&&) noexcept;
	FGeometryMesh& operator=(FGeometryMesh&&) noexcept;

	void SetData(std::span<const FGeometryVertex> InVertices, std::span<const uint32> InIndices);
	bool Upload(URenderer& Renderer);
	void Release();

	bool IsUploaded() const { return bUploaded; }
	FMeshBuffer* GetMeshBuffer() { return MeshBuffer.get(); }
	const FMeshBuffer* GetMeshBuffer() const { return MeshBuffer.get(); }

	const TArray<FGeometryVertex>& GetVertices() const { return Vertices; }
	const TArray<uint32>& GetIndices() const { return Indices; }

private:
	TArray<FGeometryVertex> Vertices;
	TArray<uint32> Indices;

	std::unique_ptr<FMeshBuffer> MeshBuffer;
	bool bUploaded = false;
};
