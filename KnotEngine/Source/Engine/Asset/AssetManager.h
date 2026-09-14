#pragma once

#include "EngineAPI.h"

#include "Asset/GeometryMesh.h"
#include "Object/ObjectPtr.h"

class FReferenceCollector;

// UObject Asset의 생성, 중복 방지와 장기 참조를 담당한다.
class ENGINE_API FAssetManager final
{
public:
	FAssetManager() = default;
	~FAssetManager();

	FAssetManager(const FAssetManager&) = delete;
	FAssetManager& operator=(const FAssetManager&) = delete;
	FAssetManager(FAssetManager&&) = delete;
	FAssetManager& operator=(FAssetManager&&) = delete;

	void Create();
	void Release();

	UGeometryMesh* GetOrCreateGeometryMesh(EGeometryMeshType MeshType);
	const TMap<EGeometryMeshType, TObjectPtr<UGeometryMesh>>& GetGeometryMeshCache() const { return GeometryMeshes; }
	void AddReferencedObjects(FReferenceCollector& Collector) const;

private:
	TMap<EGeometryMeshType, TObjectPtr<UGeometryMesh>> GeometryMeshes;
};

extern ENGINE_API FAssetManager* GAssetManager;
