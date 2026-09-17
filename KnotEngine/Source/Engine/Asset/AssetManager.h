#pragma once

#include "EngineAPI.h"

#include "Asset/AssetBinaryLoader.h"
#include "Asset/Mesh/StaticMesh.h"
#include "Asset/Material/Material.h"
#include "Asset/Texture/Texture2D.h"
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

	// 논리 경로의 Static Mesh를 Cache에서 찾고, 없으면 .kasset에서 Load한다.
	UStaticMesh* LoadStaticMesh(const FString& AssetPath);
	// 이미 Load된 Static Mesh를 반환하며 Cache Miss이면 nullptr을 반환한다.
	UStaticMesh* FindStaticMesh(const FString& AssetPath) const;

	// 논리 경로의 Material을 Cache에서 찾고, 없으면 .kasset에서 Load한다.
	UMaterial* LoadMaterial(const FString& AssetPath);
	// 이미 Load된 Material을 반환하며 Cache Miss이면 nullptr을 반환한다.
	UMaterial* FindMaterial(const FString& AssetPath) const;

	// 논리 경로의 Texture2D를 Cache에서 찾고, 없으면 .kasset에서 Load한다.
	UTexture2D* LoadTexture2D(const FString& AssetPath);
	// 이미 Load된 Texture2D를 반환하며 Cache Miss이면 nullptr을 반환한다.
	UTexture2D* FindTexture2D(const FString& AssetPath) const;

	const TMap<FString, TObjectPtr<UStaticMesh>>& GetStaticMeshes() const { return StaticMeshes; }

private:
	FAssetBinaryLoader BinaryLoader;
	TMap<FString, TObjectPtr<UStaticMesh>> StaticMeshes;
	TMap<FString, TObjectPtr<UMaterial>> Materials;
	TMap<FString, TObjectPtr<UTexture2D>> Textures;
};

extern ENGINE_API FAssetManager* GAssetManager;
