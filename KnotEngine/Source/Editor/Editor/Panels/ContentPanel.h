#pragma once

#include "Asset/AssetRegistry.h"

#include <array>

// Asset Registry의 폴더와 Asset 메타데이터를 탐색하는 Editor Content 창이다.
class FContentPanel final
{
public:
	explicit FContentPanel(FAssetRegistry& InAssetRegistry);

	void Draw();

private:
	void DrawFolderList();
	void DrawAssetList();

	FAssetRegistry& AssetRegistry;
	FString SelectedFolderPath = "/";
	FString SelectedAssetPath;
	TStaticArray<char, 128> SearchText = {};
};
