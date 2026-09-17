#pragma once

#include "EngineAPI.h"
#include "Core/Geometry/AABB.h"

#include "Render/Resource/Mesh/MeshBuffer.h"
#include "Render/Resource/Mesh/Vertex.h"

#include <span>

class IRenderDevice;

// Static Mesh LOD의 한 Draw 범위와 해당 범위가 사용하는 Material Slot을 나타낸다.
struct ENGINE_API FStaticMeshSection
{
	uint32 FirstIndex = 0;
	uint32 IndexCount = 0;
	uint32 MaterialIndex = 0;
};

// 단일 LOD Geometry의 CPU 원본 데이터와 GPU Mesh Buffer를 소유한다.
class ENGINE_API FGeometryMesh
{
public:
	FGeometryMesh() = default;
	~FGeometryMesh() = default;

	FGeometryMesh(const FGeometryMesh&) = delete;
	FGeometryMesh& operator=(const FGeometryMesh&) = delete;
	FGeometryMesh(FGeometryMesh&&) = delete;
	FGeometryMesh& operator=(FGeometryMesh&&) = delete;

	void Initialize(std::span<const FGeometryVertex> InVertices, std::span<const uint32> InIndices);
	bool InitResources(IRenderDevice& RenderDevice);
	void Release();

	const TArray<FGeometryVertex>& GetVertices() const { return Vertices; }
	const TArray<uint32>& GetIndices() const { return Indices; }
	const FAABB& GetLocalBounds() const { return LocalBounds; }
	const FMeshBuffer& GetMeshBuffer() const { return MeshBuffer; }

private:
	TArray<FGeometryVertex> Vertices;
	TArray<uint32> Indices;
	FAABB LocalBounds;

	FMeshBuffer MeshBuffer;
};

// Static Mesh LOD 하나의 CPU 원본 데이터와 GPU Mesh Buffer를 소유한다.
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
	bool InitResources(IRenderDevice& RenderDevice);
	void Release();

	bool IsValid() const { return !Vertices.empty() && LocalBounds.IsValid(); }

	const TArray<FStaticMeshVertex>& GetVertices() const { return Vertices; }
	const TArray<uint32>& GetIndices() const { return Indices; }
	const TArray<FStaticMeshSection>& GetSections() const { return Sections; }
	const FAABB& GetLocalBounds() const { return LocalBounds; }
	const FMeshBuffer& GetMeshBuffer() const { return MeshBuffer; }

private:
	TArray<FStaticMeshSection> Sections;
	TArray<FStaticMeshVertex> Vertices;
	TArray<uint32> Indices;

	FAABB LocalBounds;
	FMeshBuffer MeshBuffer;
};

// Static Mesh의 전체 LOD 배열과 모든 LOD를 포함하는 Local Bounds를 관리한다.
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
	bool InitResources(IRenderDevice& RenderDevice);
	void Release();

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
