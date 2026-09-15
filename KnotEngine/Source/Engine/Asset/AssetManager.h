#pragma once

#include "EngineAPI.h"

#include "Asset/GeometryMesh.h"
#include "Asset/StaticMesh.h"
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

	UStaticMesh* LoadStaticMesh(const FString& AssetPath, FStaticMesh&& RenderData);
	UStaticMesh* FindStaticMesh(const FString& AssetPath) const;

	void AddReferencedObjects(FReferenceCollector& Collector) const;

private:
	TMap<EGeometryMeshType, TObjectPtr<UGeometryMesh>> GeometryMeshes;
	TMap<FString, TObjectPtr<UStaticMesh>> StaticMeshes;
};

extern ENGINE_API FAssetManager* GAssetManager;
