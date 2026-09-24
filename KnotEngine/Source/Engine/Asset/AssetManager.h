#pragma once

#include "EngineAPI.h"

#include "Asset/AssetBinaryLoader.h"
#include "Asset/AssetRegistry.h"
#include "Asset/Mesh/StaticMesh.h"
#include "Asset/Material/Material.h"
#include "Asset/Texture/Texture2D.h"
#include "Object/ObjectPtr.h"
#include "Render/Resource/ResourceCommand.h"

class FReferenceCollector;
class UAsset;

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
	FAssetRegistry& GetAssetRegistry() { return AssetRegistry; }
	const FAssetRegistry& GetAssetRegistry() const { return AssetRegistry; }
	UAsset* LoadAsset(const FAssetId& AssetId);

	// 논리 경로 또는 영속 ID의 Static Mesh를 Cache에서 찾고, 없으면 .kasset에서 Load한다.
	UStaticMesh* LoadStaticMesh(const FString& AssetPath);
	UStaticMesh* LoadStaticMesh(const FAssetId& AssetId);
	UStaticMesh* FindStaticMesh(const FAssetId& AssetId) const;
	bool ReloadStaticMesh(const FAssetId& AssetId);

	// 논리 경로 또는 영속 ID의 Material을 Cache에서 찾고, 없으면 .kasset에서 Load한다.
	UMaterial* LoadMaterial(const FString& AssetPath);
	UMaterial* LoadMaterial(const FAssetId& AssetId);
	UMaterial* FindMaterial(const FAssetId& AssetId) const;

	// 논리 경로 또는 영속 ID의 Texture2D를 Cache에서 찾고, 없으면 .kasset에서 Load한다.
	UTexture2D* LoadTexture2D(const FString& AssetPath);
	UTexture2D* LoadTexture2D(const FAssetId& AssetId);
	UTexture2D* FindTexture2D(const FAssetId& AssetId) const;

	void RequestStaticMeshResource(const UStaticMesh& StaticMesh);
	void RequestMaterialResource(UMaterialInterface& Material);
	
	bool PrepareMaterialShaderReload(const TArray<FShaderKey>& ShaderKeys, TArray<FMaterialResourceCommand>& OutCommands);
	
	void DrainRenderResourceCommands(
		TArray<FStaticMeshResourceCommand>& OutStaticMeshes,
		TArray<FTextureResourceCommand>& OutTextures,
		TArray<FMaterialResourceCommand>& OutMaterials);
	void ResetRenderResourceRequests();

private:
	void RequestTextureResource(const UTexture2D& Texture);
	bool BuildMaterialResourceCommand(const UMaterialInterface& Material, uint64 Revision, FMaterialResourceCommand& OutCommand);

	struct FMaterialResourceRequest
	{
		TObjectPtr<UMaterialInterface> Material;
		uint64 Revision = 0;
	};

	FAssetRegistry AssetRegistry;
	FAssetBinaryLoader BinaryLoader;
	TMap<FAssetId, TObjectPtr<UStaticMesh>, FAssetIdHash> StaticMeshes;
	TMap<FAssetId, TObjectPtr<UMaterial>, FAssetIdHash> Materials;
	TMap<FAssetId, TObjectPtr<UTexture2D>, FAssetIdHash> Textures;

	TMap<FAssetId, uint64, FAssetIdHash> RequestedStaticMeshRevisions;
	TMap<FAssetId, FMaterialResourceRequest, FAssetIdHash> RequestedMaterialResources;
	TMap<FAssetId, uint64, FAssetIdHash> RequestedTextureRevisions;
	TArray<FStaticMeshResourceCommand> PendingStaticMeshResourceCommands;
	TArray<FMaterialResourceCommand> PendingMaterialResourceCommands;
	TArray<FTextureResourceCommand> PendingTextureResourceCommands;
};

extern ENGINE_API FAssetManager* GAssetManager;
