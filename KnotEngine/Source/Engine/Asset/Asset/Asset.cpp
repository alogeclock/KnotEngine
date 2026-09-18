#include "Asset/Asset/Asset.h"

#include "Asset/AssetManager.h"

// Registry에서 영속 ID의 현재 경로를 우선 반환하여 이동된 Asset 참조의 표시 경로를 복원한다.
const FString& UAsset::GetAssetPath() const
{
	if (GAssetManager)
	{
		if (const FAssetData* Asset = GAssetManager->GetAssetRegistry().FindAsset(AssetId))
		{
			return Asset->AssetPath;
		}
	}
	return AssetPath;
}

bool UAsset::InitializeAsset(const FAssetId& InAssetId, FString InAssetPath)
{
	if (!InAssetId.IsValid() || InAssetPath.empty())
	{
		return false;
	}

	AssetId = InAssetId;
	AssetPath = std::move(InAssetPath);
	return true;
}
