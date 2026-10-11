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

// Resource Command에서 이동받은 CPU Mesh와 GPU Buffer를 Renderer의 Static Mesh Resource Cache가 소유한다.
class ENGINE_API FStaticMeshResource final
{
public:
	FStaticMeshResource() = default;
	~FStaticMeshResource() = default;

	FStaticMeshResource(const FStaticMeshResource&) = delete;
	FStaticMeshResource& operator=(const FStaticMeshResource&) = delete;

	bool Initialize(IRenderDevice& RenderDevice, FStaticMesh&& StaticMesh, uint64 Revision);
	void Release();

	const FStaticMeshLODResource& GetLOD(SIZE_T LODIndex) const;
	SIZE_T GetLODCount() const { return LODResources.size(); }
	uint64 GetSourceRevision() const { return SourceRevision; }
	bool IsValid() const;

	const FStaticMesh& GetMeshData() const { return MeshData; }

private:
	TArray<FStaticMeshLODResource> LODResources;
	FStaticMesh MeshData;
	uint64 SourceRevision = 0;
};
