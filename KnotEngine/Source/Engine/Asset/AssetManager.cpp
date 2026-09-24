#include "Asset/AssetManager.h"

#include "Asset/Asset/Asset.h"
#include "Core/Assert.h"
#include "Object/ReferenceCollector.h"

#include <algorithm>

FAssetManager* GAssetManager = nullptr;

FAssetManager::~FAssetManager()
{
	Release();
}

// 전역 Asset Manager로 등록하고 디스크의 에셋 목록을 검색한다.
void FAssetManager::Create()
{
	checkf(!GAssetManager, "Asset Manager가 이미 생성되어 있다.");
	check(StaticMeshes.empty() && Materials.empty() && Textures.empty());
	GAssetManager = this;
	AssetRegistry.Scan();
}

// 로드된 에셋 UObject와 대기 중인 렌더 리소스 요청을 정리한다.
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
	ResetRenderResourceRequests();
	AssetRegistry.Reset();
	GAssetManager = nullptr;
}

// Registry에 기록된 에셋 타입에 따라 해당 로더로 전달한다.
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

// 이미 로드한 Static Mesh는 재사용하고, 없으면 바이너리에서 생성해 캐시한다.
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

// 이미 로드한 Material은 재사용하고, 없으면 바이너리에서 생성해 캐시한다.
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

// 이미 로드한 Texture는 재사용하고, 없으면 바이너리에서 생성해 캐시한다.
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

// Static Mesh의 Revision이 달라졌을 때만 렌더 리소스 갱신 명령을 쌓는다.
void FAssetManager::RequestStaticMeshResource(const UStaticMesh& StaticMesh)
{
	const FAssetId& AssetId = StaticMesh.GetAssetId();
	const uint64 Revision = StaticMesh.GetRevision();
	const auto Existing = RequestedStaticMeshRevisions.find(AssetId);
	if (Existing != RequestedStaticMeshRevisions.end() && Existing->second == Revision)
	{
		return;
	}

	RequestedStaticMeshRevisions[AssetId] = Revision;
	PendingStaticMeshResourceCommands.push_back({ AssetId, Revision, StaticMesh.GetMeshData() });
}

// Texture의 Revision이 달라졌을 때만 기술 정보와 Mip 데이터를 렌더 명령으로 복사한다.
void FAssetManager::RequestTextureResource(const UTexture2D& Texture)
{
	const FAssetId& AssetId = Texture.GetAssetId();
	const uint64 Revision = Texture.GetRevision();
	const auto Existing = RequestedTextureRevisions.find(AssetId);
	if (Existing != RequestedTextureRevisions.end() && Existing->second == Revision)
	{
		return;
	}

	FTextureDesc Desc;
	Desc.Width = Texture.GetWidth();
	Desc.Height = Texture.GetHeight();
	Desc.MipCount = Texture.GetMipCount();
	Desc.Format = Texture.GetFormat();
	Desc.Usage = ETextureUsage::ShaderResource;
	Desc.bSRGB = Texture.IsSRGB();
	RequestedTextureRevisions[AssetId] = Revision;
	PendingTextureResourceCommands.push_back({ AssetId, Revision, Desc, Texture.GetMips() });
}

// Material의 새 Revision만 렌더 명령으로 복사해 중복 제출을 막는다.
void FAssetManager::RequestMaterialResource(UMaterialInterface& MaterialInterface)
{
	const FAssetId& AssetId = MaterialInterface.GetAssetId();
	const uint64 Revision = MaterialInterface.GetRevision();

	const auto Existing = RequestedMaterialResources.find(AssetId);
	if (Existing != RequestedMaterialResources.end() && Existing->second.Revision == Revision)
	{
		return;
	}

	FMaterialResourceCommand Command;
	if (!BuildMaterialResourceCommand(MaterialInterface, Revision, Command))
	{
		return;
	}

	RequestedMaterialResources[AssetId] = { &MaterialInterface, Revision };
	PendingMaterialResourceCommands.push_back(std::move(Command));
}

// 현재 GT Material 값을 지정한 렌더 리소스 세대의 명령으로 복사한다.
bool FAssetManager::BuildMaterialResourceCommand(const UMaterialInterface& MaterialInterface, uint64 Revision, FMaterialResourceCommand& OutCommand)
{
	const FMaterial* Material = MaterialInterface.GetMaterial();
	if (!Material || !Material->IsValid())
	{
		return false;
	}

	OutCommand.AssetId = MaterialInterface.GetAssetId();
	OutCommand.Revision = Revision;
	OutCommand.Material = *Material;
	TArray<FTextureMaterialParameter> Textures;
	MaterialInterface.Copy(OutCommand.ScalarParameters, OutCommand.VectorParameters, Textures);
	OutCommand.Textures.reserve(Textures.size());
	for (const FTextureMaterialParameter& Parameter : Textures)
	{
		if (!Parameter.Texture)
		{
			continue;
		}
		check(Parameter.Texture->IsA(UTexture2D::StaticClass()));
		const auto& Texture = *static_cast<const UTexture2D*>(Parameter.Texture.Get());
		RequestTextureResource(Texture);
		OutCommand.Textures.push_back({ Parameter.Name, Texture.GetAssetId(), Parameter.Sampler });
	}
	return true;
}

// 사용 중인 Material 중 변경된 Shader Key에 의존하는 것만 현재 값의 후보 명령으로 준비한다.
bool FAssetManager::PrepareMaterialShaderReload(const TArray<FShaderKey>& ShaderKeys, TArray<FMaterialResourceCommand>& OutCommands)
{
	check(GAssetManager == this);
	for (const auto& [AssetId, Request] : RequestedMaterialResources)
	{
		UMaterialInterface* MaterialInterface = Request.Material.Get();
		check(MaterialInterface && MaterialInterface->GetAssetId() == AssetId);
		const FMaterial* Material = MaterialInterface->GetMaterial();
		if (!Material || !Material->IsValid())
		{
			return false;
		}
		if (!std::any_of(ShaderKeys.begin(), ShaderKeys.end(), [Material](const FShaderKey& Key)
		{
			return Key == Material->GetVertexShader() || Key == Material->GetPixelShader();
		}))
		{
			continue;
		}
		// 아직 RT에 전달하지 않은 Material은 Shader 교체 후 일반 명령으로 생성된다.
		if (std::any_of(PendingMaterialResourceCommands.begin(), PendingMaterialResourceCommands.end(), [&AssetId](const FMaterialResourceCommand& Command)
		{
			return Command.AssetId == AssetId;
		}))
		{
			continue;
		}
		FMaterialResourceCommand Command;
		if (!BuildMaterialResourceCommand(*MaterialInterface, MaterialInterface->GetRevision(), Command))
		{
			return false;
		}
		OutCommands.push_back(std::move(Command));
	}
	return true;
}

// 대기 중인 리소스 명령의 소유권을 출력 배열로 옮기고 대기 배열을 비운다.
void FAssetManager::DrainRenderResourceCommands(
	TArray<FStaticMeshResourceCommand>& OutStaticMeshes,
	TArray<FTextureResourceCommand>& OutTextures,
	TArray<FMaterialResourceCommand>& OutMaterials)
{
	OutStaticMeshes = std::move(PendingStaticMeshResourceCommands);
	OutTextures = std::move(PendingTextureResourceCommands);
	OutMaterials = std::move(PendingMaterialResourceCommands);
	PendingStaticMeshResourceCommands.clear();
	PendingTextureResourceCommands.clear();
	PendingMaterialResourceCommands.clear();
}

// Asset Resource 요청 세대와 아직 제출하지 않은 명령을 초기화한다.
void FAssetManager::ResetRenderResourceRequests()
{
	RequestedStaticMeshRevisions.clear();
	RequestedMaterialResources.clear();
	RequestedTextureRevisions.clear();
	PendingStaticMeshResourceCommands.clear();
	PendingMaterialResourceCommands.clear();
	PendingTextureResourceCommands.clear();
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
	for (const auto& [AssetId, Request] : RequestedMaterialResources)
	{
		Collector.AddReferencedObject(Request.Material);
	}
	for (const auto& Entry : Textures)
	{
		Collector.AddReferencedObject(Entry.second);
	}
}
