#include "Asset/AssetManager.h"

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
			Texture->ReleaseResources();
			GUObjectManager.Destroy(Texture);
		}
	}
	Textures.clear();
	GAssetManager = nullptr;
}

// 캐시에 없으면 Binary Loader로 Static Mesh UObject를 생성하고 장기 참조에 등록한다.
UStaticMesh* FAssetManager::LoadStaticMesh(const FString& AssetPath)
{
	check(GAssetManager == this);
	if (UStaticMesh* ExistingMesh = FindStaticMesh(AssetPath))
	{
		return ExistingMesh;
	}

	UStaticMesh* Mesh = BinaryLoader.LoadStaticMesh(AssetPath, *this);
	if (!Mesh)
	{
		return nullptr;
	}
	StaticMeshes.emplace(AssetPath, Mesh);
	return Mesh;
}

UMaterial* FAssetManager::LoadMaterial(const FString& AssetPath)
{
	check(GAssetManager == this);
	if (UMaterial* Existing = FindMaterial(AssetPath))
	{
		return Existing;
	}
	UMaterial* Material = BinaryLoader.LoadMaterial(AssetPath, *this);
	if (Material)
	{
		Materials.emplace(AssetPath, Material);
	}
	return Material;
}

UTexture2D* FAssetManager::LoadTexture2D(const FString& AssetPath)
{
	check(GAssetManager == this);
	if (UTexture2D* Existing = FindTexture2D(AssetPath))
	{
		return Existing;
	}
	UTexture2D* Texture = BinaryLoader.LoadTexture2D(AssetPath);
	if (Texture)
	{
		Textures.emplace(AssetPath, Texture);
	}
	return Texture;
}

// 캐시에서 논리 Asset 경로에 대응하는 Static Mesh를 찾는다.
UStaticMesh* FAssetManager::FindStaticMesh(const FString& AssetPath) const
{
	const auto It = StaticMeshes.find(AssetPath);
	return It != StaticMeshes.end() ? It->second.Get() : nullptr;
}

UMaterial* FAssetManager::FindMaterial(const FString& AssetPath) const
{
	const auto It = Materials.find(AssetPath);
	return It != Materials.end() ? It->second.Get() : nullptr;
}

UTexture2D* FAssetManager::FindTexture2D(const FString& AssetPath) const
{
	const auto It = Textures.find(AssetPath);
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
