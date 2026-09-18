#pragma once

#include "EngineAPI.h"

#include "Asset/Asset/AssetTypes.h"
#include "Core/CoreTypes.h"

#include <filesystem>

// Content에 존재하는 원본 파일과 파생 바이너리를 하나의 논리 Asset으로 표현한다.
struct ENGINE_API FAssetData
{
	FAssetId AssetId;
	FString Name;
	FString AssetPath;
	FString FolderPath;
	std::filesystem::path SourceFilePath;
	std::filesystem::path BinaryFilePath;
	EAssetType Type = EAssetType::Unknown;

	bool HasSourceFile() const { return !SourceFilePath.empty(); }
	bool HasBinaryFile() const { return !BinaryFilePath.empty(); }
};

// Content 디렉터리를 스캔하고 Asset ID와 현재 경로의 양방향 인덱스를 보관한다.
class ENGINE_API FAssetRegistry final
{
public:
	void Scan();
	void Reset();

	const FAssetData* FindAsset(const FAssetId& AssetId) const;
	const FAssetData* FindAsset(const FString& AssetPath) const;
	const TArray<FAssetData>& GetAssets() const { return Assets; }
	const TArray<FString>& GetFolders() const { return Folders; }

private:
	static FString MakeAssetPath(const std::filesystem::path& RelativeFilePath);
	static bool ReadAssetHeader(const std::filesystem::path& FilePath, FAssetFileHeader& OutHeader);

	TArray<FAssetData> Assets;
	TArray<FString> Folders;
	TMap<FAssetId, SIZE_T, FAssetIdHash> AssetIdIndices;
	TMap<FString, SIZE_T> AssetPathIndices;
};
