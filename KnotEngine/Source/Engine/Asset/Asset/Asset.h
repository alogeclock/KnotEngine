#pragma once

#include "EngineAPI.h"

#include "Asset/Asset/AssetTypes.h"
#include "Object/Object.h"

class FAssetBinaryLoader;
class FAssetManager;

// 디스크의 .kasset과 대응하며 영속 ID와 현재 논리 경로를 보유하는 UObject 기반 Asset이다.
UCLASS()
class ENGINE_API UAsset : public UObject
{
	GENERATED_CLASS(UAsset, UObject)

public:
	const FAssetId& GetAssetId() const { return AssetId; }
	const FString& GetAssetPath() const;
	virtual EAssetType GetAssetType() const = 0;

protected:
	bool InitializeAsset(const FAssetId& InAssetId, FString InAssetPath);

private:
	friend class FAssetManager;

	void SetAssetPath(FString InAssetPath) { AssetPath = std::move(InAssetPath); }

	FAssetId AssetId;
	UPROPERTY(NoEdit) FString AssetPath;
};
