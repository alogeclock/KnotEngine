#pragma once

#include "EngineAPI.h"

#include "Object/Object.h"
#include "Render/Resource/Mesh.h"

class FAssetBinaryLoader;

// Static Mesh .kasset 전체에 한 번 저장되는 고정 크기 헤더다.
struct FStaticMeshBinaryHeader
{
	inline static constexpr char MagicValue[4] = { 'K', 'M', 'S', 'H' };
	inline static constexpr uint32 CurrentVersion = 1;
	inline static constexpr uint32 MaxLODCount = 16;

	char Magic[4];
	uint32 Version;
	uint32 VertexStride;
	uint32 LODCount;
};
static_assert(sizeof(FStaticMeshBinaryHeader) == 16);

// Static Mesh의 각 LOD 데이터 앞에 저장되는 배열 크기다.
struct FStaticMeshLODBinaryHeader
{
	uint32 VertexCount;
	uint32 IndexCount;
};
static_assert(sizeof(FStaticMeshLODBinaryHeader) == 8);

// 경로로 식별되는 Static Mesh UObject Asset. 실제 LOD와 GPU 리소스는 FStaticMesh가 소유한다.
UCLASS()
class ENGINE_API UStaticMesh final : public UObject
{
	GENERATED_CLASS(UStaticMesh, UObject)

public:
	const FString& GetAssetPath() const { return AssetPath; }
	FStaticMesh& GetRenderData() { return RenderData; }
	const FStaticMesh& GetRenderData() const { return RenderData; }

private:
	friend class FAssetBinaryLoader;
	bool Initialize(FString InAssetPath, FStaticMesh&& InRenderData);

	UPROPERTY(NoEdit) FString AssetPath;
	FStaticMesh RenderData;
};
