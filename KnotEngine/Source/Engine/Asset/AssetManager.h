#pragma once

#include "EngineAPI.h"

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

	void AddReferencedObjects(FReferenceCollector& Collector) const;

	UStaticMesh* LoadStaticMesh(const FString& AssetPath);
	UStaticMesh* FindStaticMesh(const FString& AssetPath) const;
	const TMap<FString, TObjectPtr<UStaticMesh>>& GetStaticMeshes() const { return StaticMeshes; }

private:
	UStaticMesh* RegisterStaticMesh(const FString& AssetPath, FStaticMesh&& RenderData);

	TMap<FString, TObjectPtr<UStaticMesh>> StaticMeshes;
};

extern ENGINE_API FAssetManager* GAssetManager;
