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
	check(StaticMeshes.empty());
	GAssetManager = this;

	panicf(LoadStaticMesh("/Engine/Geometry/Cube"), "내장 Cube Static Mesh를 불러오지 못했다.");
	panicf(LoadStaticMesh("/Engine/Geometry/Sphere"), "내장 Sphere Static Mesh를 불러오지 못했다.");
	panicf(LoadStaticMesh("/Engine/Geometry/Quad"), "내장 Quad Static Mesh를 불러오지 못했다.");
	panicf(LoadStaticMesh("/Engine/Geometry/Cylinder"), "내장 Cylinder Static Mesh를 불러오지 못했다.");
	panicf(LoadStaticMesh("/Engine/Geometry/Capsule"), "내장 Capsule Static Mesh를 불러오지 못했다.");
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

	UStaticMesh* Mesh = BinaryLoader.LoadStaticMesh(AssetPath);
	if (!Mesh)
	{
		return nullptr;
	}
	StaticMeshes.emplace(AssetPath, Mesh);
	return Mesh;
}

// 캐시에서 논리 Asset 경로에 대응하는 Static Mesh를 찾는다.
UStaticMesh* FAssetManager::FindStaticMesh(const FString& AssetPath) const
{
	const auto It = StaticMeshes.find(AssetPath);
	return It != StaticMeshes.end() ? It->second.Get() : nullptr;
}

// FAssetManager는 UObject가 아니라 일반 C++ 객체이므로, 자동으로 수집되지 않는다.
void FAssetManager::AddReferencedObjects(FReferenceCollector& Collector) const
{
	for (const auto& Entry : StaticMeshes)
	{
		Collector.AddReferencedObject(Entry.second);
	}
}
