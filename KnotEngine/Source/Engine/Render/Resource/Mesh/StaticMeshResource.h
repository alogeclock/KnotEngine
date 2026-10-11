#pragma once

#include "EngineAPI.h"
#include "Asset/Mesh/StaticMesh.h"
#include "Render/Resource/Mesh/MeshBuffer.h"

class IRenderDevice;

struct ENGINE_API FStaticMeshSectionResource
{
	uint32 FirstIndex = 0;
	uint32 IndexCount = 0;
	uint32 MaterialIndex = 0;
};

// Static Mesh LOD 하나의 GPU Buffer와 Draw Section 복사본이다.
class ENGINE_API FStaticMeshLODResource final
{
public:
	const FMeshBuffer& GetMeshBuffer() const { return MeshBuffer; }
	const TArray<FStaticMeshSectionResource>& GetSections() const { return Sections; }
	bool IsValid() const { return MeshBuffer.IsValid(); }

private:
	friend class FStaticMeshResource;
	FMeshBuffer MeshBuffer;
	TArray<FStaticMeshSectionResource> Sections;
};

// GT Asset과 불변 CPU Mesh를 공유하고 GPU Buffer는 RT의 Resource Cache에서만 소유한다.
class ENGINE_API FStaticMeshResource final
{
public:
	FStaticMeshResource() = default;
	~FStaticMeshResource() = default;

	FStaticMeshResource(const FStaticMeshResource&) = delete;
	FStaticMeshResource& operator=(const FStaticMeshResource&) = delete;

	bool Initialize(IRenderDevice& RenderDevice, std::shared_ptr<const FStaticMesh> StaticMesh, uint64 Revision);
	void Release();

	const FStaticMeshLODResource& GetLOD(SIZE_T LODIndex) const;
	SIZE_T GetLODCount() const { return LODResources.size(); }
	uint64 GetSourceRevision() const { return SourceRevision; }
	bool IsValid() const;

	const std::shared_ptr<const FStaticMesh>& GetMeshData() const { return MeshData; }

private:
	TArray<FStaticMeshLODResource> LODResources;
	std::shared_ptr<const FStaticMesh> MeshData;
	uint64 SourceRevision = 0;
};
