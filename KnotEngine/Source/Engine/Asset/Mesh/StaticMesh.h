#pragma once

#include "EngineAPI.h"

#include "Asset/Asset/Asset.h"
#include "Asset/Material/MaterialInterface.h"
#include "Core/Archive/Archive.h"
#include "Core/Geometry/AABB.h"
#include "Core/Name.h"
#include "Object/Object.h"
#include "Render/Resource/Mesh/Vertex.h"

#include <span>

class FAssetBinaryLoader;
class FReferenceCollector;

// Static Mesh LOD의 한 Draw 범위와 해당 범위가 사용하는 Material Slot을 나타낸다.
struct ENGINE_API FStaticMeshSection
{
	uint32 FirstIndex = 0;
	uint32 IndexCount = 0;
	uint32 MaterialIndex = 0;
};

// Static Mesh LOD 하나의 CPU Vertex, Index, Section과 Local Bounds를 소유한다.
class ENGINE_API FStaticMeshLOD
{
public:
	FStaticMeshLOD() = default;
	~FStaticMeshLOD() = default;

	FStaticMeshLOD(const FStaticMeshLOD&) = delete;
	FStaticMeshLOD& operator=(const FStaticMeshLOD&) = delete;
	FStaticMeshLOD(FStaticMeshLOD&&) noexcept = default;
	FStaticMeshLOD& operator=(FStaticMeshLOD&&) noexcept = default;

	bool Initialize(std::span<const FStaticMeshVertex> InVertices, std::span<const uint32> InIndices, std::span<const FStaticMeshSection> InSections = {});
	bool IsValid() const { return !Vertices.empty() && LocalBounds.IsValid(); }

	const TArray<FStaticMeshVertex>& GetVertices() const { return Vertices; }
	const TArray<uint32>& GetIndices() const { return Indices; }
	const TArray<FStaticMeshSection>& GetSections() const { return Sections; }
	const FAABB& GetLocalBounds() const { return LocalBounds; }

private:
	TArray<FStaticMeshSection> Sections;
	TArray<FStaticMeshVertex> Vertices;
	TArray<uint32> Indices;
	FAABB LocalBounds;
};

// Static Mesh의 CPU LOD 배열과 모든 LOD를 포함하는 Local Bounds를 소유한다.
class ENGINE_API FStaticMesh
{
public:
	FStaticMesh() = default;
	~FStaticMesh() = default;

	FStaticMesh(const FStaticMesh&) = delete;
	FStaticMesh& operator=(const FStaticMesh&) = delete;
	FStaticMesh(FStaticMesh&&) noexcept = default;
	FStaticMesh& operator=(FStaticMesh&&) noexcept = default;

	bool AddLOD(std::span<const FStaticMeshVertex> Vertices, std::span<const uint32> Indices, std::span<const FStaticMeshSection> Sections = {});
	bool IsValid() const { return !LODs.empty() && LocalBounds.IsValid(); }

	SIZE_T GetLODCount() const { return LODs.size(); }
	FStaticMeshLOD& GetLOD(SIZE_T LODIndex);
	const FStaticMeshLOD& GetLOD(SIZE_T LODIndex) const;
	const TArray<FStaticMeshLOD>& GetLODs() const { return LODs; }
	const FAABB& GetLocalBounds() const { return LocalBounds; }

private:
	TArray<FStaticMeshLOD> LODs;
	FAABB LocalBounds;
};

// Static Mesh의 이름 있는 Material Slot과 Asset 기본 Material을 저장한다.
struct ENGINE_API FStaticMaterial
{
	FName SlotName;
	TObjectPtr<UMaterialInterface> Material;
};

// Static Mesh .kasset Payload 전체에 한 번 저장되는 고정 크기 헤더다.
struct FStaticMeshPayloadHeader
{
	inline static constexpr uint32 CurrentVersion = 2;
	inline static constexpr uint32 MaxLODCount = 5;

	uint32 VertexStride;
	uint32 LODCount;
	uint32 MaterialCount;
};
static_assert(sizeof(FStaticMeshPayloadHeader) == 12);

inline FArchive& operator<<(FArchive& Ar, FStaticMeshPayloadHeader& Header)
{
	Ar << Header.VertexStride;
	Ar << Header.LODCount;
	Ar << Header.MaterialCount;
	return Ar;
}

// Static Mesh의 각 LOD 데이터 앞에 저장되는 배열 크기다.
struct FStaticMeshLODPayloadHeader
{
	uint32 VertexCount;
	uint32 IndexCount;
	uint32 SectionCount;
};
static_assert(sizeof(FStaticMeshLODPayloadHeader) == 12);

inline FArchive& operator<<(FArchive& Ar, FStaticMeshLODPayloadHeader& Header)
{
	Ar << Header.VertexCount;
	Ar << Header.IndexCount;
	Ar << Header.SectionCount;
	return Ar;
}

// 영속 Asset ID로 식별되며 CPU LOD와 Material Slot을 소유한다. GPU 리소스는 Renderer가 별도로 관리한다.
UCLASS()
class ENGINE_API UStaticMesh final : public UAsset
{
	GENERATED_CLASS(UStaticMesh, UAsset)

public:
	EAssetType GetAssetType() const override { return EAssetType::StaticMesh; }
	FStaticMesh& GetMeshData() { return MeshData; }
	const FStaticMesh& GetMeshData() const { return MeshData; }
	uint64 GetRevision() const { return Revision; }

	SIZE_T GetMaterialCount() const { return StaticMaterials.size(); }
	UMaterialInterface* GetMaterial(SIZE_T MaterialIndex) const;
	const TArray<FStaticMaterial>& GetStaticMaterials() const { return StaticMaterials; }

	void AddReferencedObjects(FReferenceCollector& Collector) override;

private:
	friend class FAssetBinaryLoader;
	friend class FAssetManager;
	bool Initialize(
		const FAssetId& InAssetId,
		FString InAssetPath,
		FStaticMesh&& InMeshData,
		TArray<FStaticMaterial>&& InStaticMaterials = {});
	bool Reload(FStaticMesh&& InMeshData, TArray<FStaticMaterial>&& InStaticMaterials);

	FStaticMesh MeshData;
	TArray<FStaticMaterial> StaticMaterials;
	uint64 Revision = 0;
};
