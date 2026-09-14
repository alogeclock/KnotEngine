#pragma once

#include "EngineAPI.h"
#include "Core/Geometry/AABB.h"

#include "Render/Resource/VertexTypes.h"

#include <span>

class FMeshBuffer;
class URenderer;

enum class EGeometryMeshType : uint8
{
	Quad,
	Sphere,
	Cube,
};

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
	static std::shared_ptr<FGeometryMesh> Create(EGeometryMeshType MeshType);

	void SetData(std::span<const FGeometryVertex> InVertices, std::span<const uint32> InIndices);
	bool Upload(URenderer& Renderer);
	void Release();

	bool IsUploaded() const { return bUploaded; }
	FMeshBuffer* GetMeshBuffer() { return MeshBuffer.get(); }
	const FMeshBuffer* GetMeshBuffer() const { return MeshBuffer.get(); }

	const TArray<FGeometryVertex>& GetVertices() const { return Vertices; }
	const TArray<uint32>& GetIndices() const { return Indices; }
	const FAABB& GetLocalBounds() const { return LocalBounds; }

private:
	TArray<FGeometryVertex> Vertices;
	TArray<uint32> Indices;
	FAABB LocalBounds;

	std::unique_ptr<FMeshBuffer> MeshBuffer;
	bool bUploaded = false;
};
