#include "Asset/AssetManager.h"

#include "Asset/Asset/Asset.h"
#include "Core/Assert.h"
#include "Object/ReferenceCollector.h"

FAssetManager* GAssetManager = nullptr;

FAssetManager::~FAssetManager()
{
	Release();
}

void FAssetManager::Create()
{
	checkf(!GAssetManager, "Asset Manager가 이미 생성되어 있다.");
	check(StaticMeshes.empty() && Materials.empty() && Textures.empty());
	GAssetManager = this;
	AssetRegistry.Scan();
}

void FAssetManager::Release()
{
	if (GAssetManager != this)
	{
		return;
	}

	for (const auto& Entry : StaticMeshes)
	{
		if (UStaticMesh* Mesh = Entry.second.Get())
		{
			GUObjectManager.Destroy(Mesh);
		}
	}
	StaticMeshes.clear();
	for (const auto& Entry : Materials)
	{
		if (UMaterial* Material = Entry.second.Get())
		{
			GUObjectManager.Destroy(Material);
		}
	}
	Materials.clear();
	for (const auto& Entry : Textures)
	{
		if (UTexture2D* Texture = Entry.second.Get())
		{
			GUObjectManager.Destroy(Texture);
		}
	}
	Textures.clear();
	AssetRegistry.Reset();
	GAssetManager = nullptr;
}

UAsset* FAssetManager::LoadAsset(const FAssetId& AssetId)
{
	const FAssetData* Asset = AssetRegistry.FindAsset(AssetId);
	if (!Asset)
	{
		return nullptr;
	}
	switch (Asset->Type)
	{
	case EAssetType::StaticMesh: return LoadStaticMesh(AssetId);
	case EAssetType::Material: return LoadMaterial(AssetId);
	case EAssetType::Texture2D: return LoadTexture2D(AssetId);
	default: return nullptr;
	}
}

UStaticMesh* FAssetManager::LoadStaticMesh(const FString& AssetPath)
{
	const FAssetData* Asset = AssetRegistry.FindAsset(AssetPath);
	return Asset && Asset->Type == EAssetType::StaticMesh ? LoadStaticMesh(Asset->AssetId) : nullptr;
}

UStaticMesh* FAssetManager::LoadStaticMesh(const FAssetId& AssetId)
{
	check(GAssetManager == this);
	const FAssetData* Asset = AssetRegistry.FindAsset(AssetId);
	if (!Asset || Asset->Type != EAssetType::StaticMesh)
	{
		return nullptr;
	}
	if (UStaticMesh* Existing = FindStaticMesh(AssetId))
	{
		Existing->SetAssetPath(Asset->AssetPath);
		return Existing;
	}
	UStaticMesh* Mesh = BinaryLoader.LoadStaticMesh(*Asset, *this);
	if (!Mesh)
	{
		return nullptr;
	}
	StaticMeshes.emplace(AssetId, Mesh);
	return Mesh;
}

UMaterial* FAssetManager::LoadMaterial(const FString& AssetPath)
{
	const FAssetData* Asset = AssetRegistry.FindAsset(AssetPath);
	return Asset && Asset->Type == EAssetType::Material ? LoadMaterial(Asset->AssetId) : nullptr;
}

UMaterial* FAssetManager::LoadMaterial(const FAssetId& AssetId)
{
	check(GAssetManager == this);
	const FAssetData* Asset = AssetRegistry.FindAsset(AssetId);
	if (!Asset || Asset->Type != EAssetType::Material)
	{
		return nullptr;
	}
	if (UMaterial* Existing = FindMaterial(AssetId))
	{
		Existing->SetAssetPath(Asset->AssetPath);
		return Existing;
	}
	UMaterial* Material = BinaryLoader.LoadMaterial(*Asset, *this);
	if (Material)
	{
		Materials.emplace(AssetId, Material);
	}
	return Material;
}

UTexture2D* FAssetManager::LoadTexture2D(const FString& AssetPath)
{
	const FAssetData* Asset = AssetRegistry.FindAsset(AssetPath);
	return Asset && Asset->Type == EAssetType::Texture2D ? LoadTexture2D(Asset->AssetId) : nullptr;
}

UTexture2D* FAssetManager::LoadTexture2D(const FAssetId& AssetId)
{
	check(GAssetManager == this);
	const FAssetData* Asset = AssetRegistry.FindAsset(AssetId);
	if (!Asset || Asset->Type != EAssetType::Texture2D)
	{
		return nullptr;
	}
	if (UTexture2D* Existing = FindTexture2D(AssetId))
	{
		Existing->SetAssetPath(Asset->AssetPath);
		return Existing;
	}
	UTexture2D* Texture = BinaryLoader.LoadTexture2D(*Asset);
	if (Texture)
	{
		Textures.emplace(AssetId, Texture);
	}
	return Texture;
}

UStaticMesh* FAssetManager::FindStaticMesh(const FAssetId& AssetId) const
{
	const auto It = StaticMeshes.find(AssetId);
	return It != StaticMeshes.end() ? It->second.Get() : nullptr;
}

// 이미 로드된 Static Mesh UObject와 AssetId를 유지하면서 최신 .kasset의 LOD와 Material 데이터를 반영한다.
bool FAssetManager::ReloadStaticMesh(const FAssetId& AssetId)
{
	check(GAssetManager == this);
	UStaticMesh* ExistingMesh = FindStaticMesh(AssetId);
	if (!ExistingMesh)
	{
		return true;
	}

	const FAssetData* Asset = AssetRegistry.FindAsset(AssetId);
	if (!Asset || Asset->Type != EAssetType::StaticMesh)
	{
		return false;
	}
	UStaticMesh* ImportedMesh = BinaryLoader.LoadStaticMesh(*Asset, *this);
	if (!ImportedMesh)
	{
		return false;
	}
	check(ImportedMesh->GetAssetId() == ExistingMesh->GetAssetId());
	const bool bReloaded = ExistingMesh->Reload(std::move(ImportedMesh->MeshData), std::move(ImportedMesh->StaticMaterials));
	if (bReloaded)
	{
		ExistingMesh->SetAssetPath(Asset->AssetPath);
	}
	GUObjectManager.Destroy(ImportedMesh);
	return bReloaded;
}

UMaterial* FAssetManager::FindMaterial(const FAssetId& AssetId) const
{
	const auto It = Materials.find(AssetId);
	return It != Materials.end() ? It->second.Get() : nullptr;
}

UTexture2D* FAssetManager::FindTexture2D(const FAssetId& AssetId) const
{
	const auto It = Textures.find(AssetId);
	return It != Textures.end() ? It->second.Get() : nullptr;
}

// FAssetManager는 UObject가 아니라 일반 C++ 객체이므로, 자동으로 수집되지 않는다.
void FAssetManager::AddReferencedObjects(FReferenceCollector& Collector) const
{
	for (const auto& Entry : StaticMeshes)
	{
		Collector.AddReferencedObject(Entry.second);
	}
	for (const auto& Entry : Materials)
	{
		Collector.AddReferencedObject(Entry.second);
	}
	for (const auto& Entry : Textures)
	{
		Collector.AddReferencedObject(Entry.second);
	}
}
