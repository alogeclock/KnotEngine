#pragma once

#include "Core/CoreTypes.h"

#include <filesystem>

enum class EAssetType : uint8
{
	Unknown,
	StaticMesh,
};

// Content에 존재하는 원본 파일과 파생 바이너리를 하나의 논리 Asset으로 표현한다.
struct FAssetData
{
	FString Name;
	FString AssetPath;
	FString FolderPath;
	std::filesystem::path SourceFilePath;
	std::filesystem::path BinaryFilePath;
	EAssetType Type = EAssetType::Unknown;

	bool HasSourceFile() const { return !SourceFilePath.empty(); }
	bool HasBinaryFile() const { return !BinaryFilePath.empty(); }
};

// Contents 디렉터리를 스캔하고 UObject를 로드하지 않은 Asset 메타데이터를 보관한다.
class FAssetRegistry final
{
public:
	void Scan();
	void Reset();

	const FAssetData* FindAsset(const FString& AssetPath) const;
	const TArray<FAssetData>& GetAssets() const { return Assets; }
	const TArray<FString>& GetFolders() const { return Folders; }

private:
	TArray<FAssetData> Assets;
	TArray<FString> Folders;
	TMap<FString, SIZE_T> AssetIndices;
};
