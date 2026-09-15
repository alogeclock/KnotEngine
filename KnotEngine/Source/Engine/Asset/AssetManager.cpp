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
	check(GeometryMeshes.empty() && StaticMeshes.empty());
	GAssetManager = this;
	GetOrCreateGeometryMesh(EGeometryMeshType::Cube);
	GetOrCreateGeometryMesh(EGeometryMeshType::Sphere);
	GetOrCreateGeometryMesh(EGeometryMeshType::Quad);
}

void FAssetManager::Release()
{
	if (GAssetManager != this)
	{
		return;
	}

	for (const auto& Entry : GeometryMeshes)
	{
		if (UGeometryMesh* Mesh = Entry.second.Get())
		{
			GUObjectManager.Destroy(Mesh);
		}
	}
	GeometryMeshes.clear();
	for (const auto& Entry : StaticMeshes)
	{
		if (UStaticMesh* Mesh = Entry.second.Get())
		{
			GUObjectManager.Destroy(Mesh);
		}
	}
	StaticMeshes.clear();
	GAssetManager = nullptr;
}

UGeometryMesh* FAssetManager::GetOrCreateGeometryMesh(EGeometryMeshType MeshType)
{
	check(GAssetManager == this);
	const auto It = GeometryMeshes.find(MeshType);
	if (It != GeometryMeshes.end())
	{
		return It->second.Get();
	}

	UGeometryMesh* Mesh = GUObjectManager.Create<UGeometryMesh>();
	Mesh->Initialize(MeshType);
	GeometryMeshes.emplace(MeshType, Mesh);
	return Mesh;
}

UStaticMesh* FAssetManager::LoadStaticMesh(const FString& AssetPath, FStaticMesh&& RenderData)
{
	check(GAssetManager == this);
	if (UStaticMesh* ExistingMesh = FindStaticMesh(AssetPath))
	{
		return ExistingMesh;
	}

	UStaticMesh* Mesh = GUObjectManager.Create<UStaticMesh>();
	if (!Mesh->Initialize(AssetPath, std::move(RenderData)))
	{
		GUObjectManager.Destroy(Mesh);
		return nullptr;
	}
	StaticMeshes.emplace(AssetPath, Mesh);
	return Mesh;
}

UStaticMesh* FAssetManager::FindStaticMesh(const FString& AssetPath) const
{
	const auto It = StaticMeshes.find(AssetPath);
	return It != StaticMeshes.end() ? It->second.Get() : nullptr;
}

// FAssetManager는 UObject가 아니라 일반 C++ 객체이므로, 자동으로 수집되지 않는다.
void FAssetManager::AddReferencedObjects(FReferenceCollector& Collector) const
{
	for (const auto& Entry : GeometryMeshes)
	{
		Collector.AddReferencedObject(Entry.second);
	}
	for (const auto& Entry : StaticMeshes)
	{
		Collector.AddReferencedObject(Entry.second);
	}
}
