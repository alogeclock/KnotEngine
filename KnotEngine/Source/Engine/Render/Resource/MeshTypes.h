#pragma once

#include "EngineAPI.h"
#include "Core/Geometry/AABB.h"

#include "Render/Resource/MeshResources.h"
#include "Render/Resource/VertexTypes.h"

#include <memory>
#include <span>

class FResourceManager;

enum class EGeometryMeshType : uint8
{
	Quad,
	Sphere,
	Cube,
};

// 단일 LOD 디버그 Geometry의 CPU 원본 데이터와 GPU Mesh Buffer 슬롯을 소유한다.
// GPU Buffer 생성은 FResourceManager가 담당하고, 활성 상태와 해제는 Geometry Mesh가 관리한다.
class ENGINE_API FGeometryMesh
{
public:
	FGeometryMesh() = default;
	~FGeometryMesh() = default;

	FGeometryMesh(const FGeometryMesh&) = delete;
	FGeometryMesh& operator=(const FGeometryMesh&) = delete;
	FGeometryMesh(FGeometryMesh&&) = delete;
	FGeometryMesh& operator=(FGeometryMesh&&) = delete;

	static std::shared_ptr<FGeometryMesh> Create(EGeometryMeshType MeshType);

	void Initialize(std::span<const FGeometryVertex> InVertices, std::span<const uint32> InIndices);
	void Release();

	const TArray<FGeometryVertex>& GetVertices() const { return Vertices; }
	const TArray<uint32>& GetIndices() const { return Indices; }
	const FAABB& GetLocalBounds() const { return LocalBounds; }
	const FMeshBuffer& GetMeshBuffer() const { return MeshBuffer; }

private:
	friend class FResourceManager;

	TArray<FGeometryVertex> Vertices;
	TArray<uint32> Indices;
	FAABB LocalBounds;

	FMeshBuffer MeshBuffer;
};
