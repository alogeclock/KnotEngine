#pragma once

#include "EngineAPI.h"

#include "Object/Object.h"
#include "Render/Resource/MeshTypes.h"

class FAssetManager;

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
	friend class FAssetManager;
	bool Initialize(FString InAssetPath, FStaticMesh&& InRenderData);

	UPROPERTY(NoEdit) FString AssetPath;
	FStaticMesh RenderData;
};
