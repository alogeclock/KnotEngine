#pragma once

#include "Asset/AssetTypes.h"
#include "Core/CoreTypes.h"

#include <filesystem>

// 한 번의 Import로 생성된 개별 Runtime Asset의 논리 경로와 종류다.
struct FImportedAsset
{
	FString AssetPath;
	EAssetType Type = EAssetType::Unknown;
};

// Import 성공 여부와 생성된 Asset, 경고 및 오류를 함께 반환하는 결과다.
struct FAssetImportResult
{
	bool bSucceeded = false;
	TArray<FImportedAsset> ImportedAssets;
	TArray<FString> Warnings;
	FString Error;
};

// GLB Source를 Runtime 전용 Mesh, Material, Texture .kasset으로 변환하는 Editor 전용 Importer다.
class FAssetImporter final
{
public:
	// 단일 GLB를 읽어 Mesh, Material, Texture .kasset으로 변환한다.
	FAssetImportResult ImportGLB(const std::filesystem::path& SourceFilePath, const FString& DestinationAssetPath, bool bCreateTypeFolders = true) const;

	// Content 아래의 모든 GLB를 탐색하여 변경된 Source Asset을 Import한다.
	bool ImportAllGLB() const;
};
