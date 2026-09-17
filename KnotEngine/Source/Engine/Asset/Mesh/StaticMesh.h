#pragma once

#include "EngineAPI.h"

#include "Asset/Material/MaterialInterface.h"
#include "Core/Archive.h"
#include "Core/Name.h"
#include "Object/Object.h"
#include "Render/Resource/Mesh/Mesh.h"

class FAssetBinaryLoader;
class FReferenceCollector;

// Static Mesh의 이름 있는 Material Slot과 Asset 기본 Material을 저장한다.
struct ENGINE_API FStaticMaterial
{
	FName SlotName;
	TObjectPtr<UMaterialInterface> Material;
};

// Static Mesh .kasset Payload 전체에 한 번 저장되는 고정 크기 헤더다.
struct FStaticMeshPayloadHeader
{
	inline static constexpr uint32 CurrentVersion = 1;
	inline static constexpr uint32 MaxLODCount = 16;

	uint32 VertexStride;
	uint32 LODCount;
	uint32 MaterialCount;
};
static_assert(sizeof(FStaticMeshPayloadHeader) == 12);

inline FArchive& operator<<(FArchive& Ar, FStaticMeshPayloadHeader& Header)
{
	Ar << Header.VertexStride;
	Ar << Header.LODCount;
	Ar << Header.MaterialCount;
	return Ar;
}

// Static Mesh의 각 LOD 데이터 앞에 저장되는 배열 크기다.
struct FStaticMeshLODPayloadHeader
{
	uint32 VertexCount;
	uint32 IndexCount;
	uint32 SectionCount;
};
static_assert(sizeof(FStaticMeshLODPayloadHeader) == 12);

inline FArchive& operator<<(FArchive& Ar, FStaticMeshLODPayloadHeader& Header)
{
	Ar << Header.VertexCount;
	Ar << Header.IndexCount;
	Ar << Header.SectionCount;
	return Ar;
}

// 경로로 식별되는 Static Mesh UObject Asset. 실제 LOD와 GPU 리소스는 FStaticMesh가 소유한다.
UCLASS()
class ENGINE_API UStaticMesh final : public UObject
{
	GENERATED_CLASS(UStaticMesh, UObject)

public:
	const FString& GetAssetPath() const { return AssetPath; }
	FStaticMesh& GetRenderData() { return RenderData; }
	const FStaticMesh& GetRenderData() const { return RenderData; }

	SIZE_T GetMaterialCount() const { return StaticMaterials.size(); }
	UMaterialInterface* GetMaterial(SIZE_T MaterialIndex) const;
	const TArray<FStaticMaterial>& GetStaticMaterials() const { return StaticMaterials; }

	void AddReferencedObjects(FReferenceCollector& Collector) override;

private:
	friend class FAssetBinaryLoader;
	bool Initialize(FString InAssetPath, FStaticMesh&& InRenderData, TArray<FStaticMaterial>&& InStaticMaterials = {});

	UPROPERTY(NoEdit) FString AssetPath;

	FStaticMesh RenderData;
	TArray<FStaticMaterial> StaticMaterials;
};
