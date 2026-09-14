#pragma once

#include "EngineAPI.h"

#include "Render/Resource/MeshTypes.h"

class FMeshBuffer;
class IRenderDevice;

// Engine 공용 리소스를 캐싱하고 Render Device 의존적인 GPU 리소스 생성을 담당한다.
// Mesh Buffer는 각 Mesh가 소유하며, Resource Manager는 캐싱한 Mesh의 공유 수명만 관리한다.
class ENGINE_API FResourceManager
{
public:
	explicit FResourceManager(IRenderDevice& InRenderDevice);
	~FResourceManager();

	FResourceManager(const FResourceManager&) = delete;
	FResourceManager& operator=(const FResourceManager&) = delete;
	FResourceManager(FResourceManager&&) = delete;
	FResourceManager& operator=(FResourceManager&&) = delete;

	void Create();

	std::shared_ptr<FGeometryMesh> GetOrCreateGeometryMesh(EGeometryMeshType MeshType);

	FMeshBuffer* GetOrCreateMeshBuffer(FGeometryMesh& Mesh);
	void Release();

private:
	IRenderDevice& RenderDevice;
	
	TMap<EGeometryMeshType, std::shared_ptr<FGeometryMesh>> GeometryMeshCache;
};

extern ENGINE_API FResourceManager* GResourceManager;
