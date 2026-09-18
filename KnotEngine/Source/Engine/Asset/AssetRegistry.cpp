#include "Asset/AssetRegistry.h"

#include "Core/IO/Paths.h"
#include "Core/Log.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <system_error>

// 확장자를 제외한 Content 상대 경로를 논리 Asset 경로로 변환한다.
FString FAssetRegistry::MakeAssetPath(const std::filesystem::path& RelativeFilePath)
{
	std::filesystem::path RelativeAssetPath = RelativeFilePath;
	RelativeAssetPath.replace_extension();
	return "/" + FPaths::ToUtf8(RelativeAssetPath.generic_wstring());
}

// 현재 컨테이너 형식의 .kasset 헤더를 읽는다.
bool FAssetRegistry::ReadAssetHeader(const std::filesystem::path& FilePath, FAssetFileHeader& OutHeader)
{
	std::ifstream Stream(FilePath, std::ios::binary);
	return Stream.read(reinterpret_cast<char*>(&OutHeader), sizeof(OutHeader)) &&
		std::memcmp(OutHeader.Magic, FAssetFileHeader::MagicValue, sizeof(OutHeader.Magic)) == 0 &&
		OutHeader.ContainerVersion == FAssetFileHeader::CurrentVersion && OutHeader.AssetId.IsValid();
}

// Content의 원본과 바이너리를 다시 찾아 Asset 목록 및 ID/경로 인덱스를 교체한다.
void FAssetRegistry::Scan()
{
	Reset();

	const std::filesystem::path ContentPath(FPaths::ContentDir());
	std::error_code FileSystemError;
	if (!std::filesystem::exists(ContentPath, FileSystemError) || FileSystemError)
	{
		KE_LOG(LogAssetRegistry, Warning, "Content 디렉터리를 찾을 수 없다. Path={}", FPaths::ToUtf8(ContentPath.generic_wstring()));
		return;
	}

	TMap<FString, FAssetData> ScannedAssets;
	TSet<FString> FolderSet;
	FolderSet.emplace("/");
	std::filesystem::recursive_directory_iterator Iterator(ContentPath, std::filesystem::directory_options::skip_permission_denied, FileSystemError);
	const std::filesystem::recursive_directory_iterator End;
	while (Iterator != End)
	{
		if (FileSystemError)
		{
			KE_LOG(LogAssetRegistry, Warning, "Content 항목을 읽지 못했다. Error={}", FileSystemError.message());
			FileSystemError.clear();
			Iterator.increment(FileSystemError);
			continue;
		}

		const std::filesystem::directory_entry& Entry = *Iterator;
		if (Entry.is_directory(FileSystemError) && !FileSystemError)
		{
			const std::filesystem::path RelativePath = std::filesystem::relative(Entry.path(), ContentPath, FileSystemError);
			if (!FileSystemError)
			{
				FolderSet.emplace("/" + FPaths::ToUtf8(RelativePath.generic_wstring()));
			}
		}
		else if (Entry.is_regular_file(FileSystemError) && !FileSystemError)
		{
			FString Extension = FPaths::ToUtf8(Entry.path().extension().generic_wstring());
			std::transform(Extension.begin(), Extension.end(), Extension.begin(), [](unsigned char Character)
			{
				return static_cast<char>(std::tolower(Character));
			});

			if (Extension == ".glb" || Extension == ".kasset")
			{
				const std::filesystem::path RelativePath = std::filesystem::relative(Entry.path(), ContentPath, FileSystemError);
				if (!FileSystemError)
				{
					const FString AssetPath = MakeAssetPath(RelativePath);
					FAssetData& Asset = ScannedAssets[AssetPath];
					Asset.Name = FPaths::ToUtf8(RelativePath.stem().wstring());
					Asset.AssetPath = AssetPath;
					Asset.FolderPath = FPaths::GetPath(AssetPath);
					if (Extension == ".glb")
					{
						Asset.SourceFilePath = Entry.path();
					}
					else
					{
						FAssetFileHeader Header = {};
						Asset.BinaryFilePath = Entry.path();
						if (ReadAssetHeader(Entry.path(), Header))
						{
							Asset.AssetId = Header.AssetId;
							Asset.Type = Header.AssetType;
						}
					}
				}
			}
		}
		FileSystemError.clear();
		Iterator.increment(FileSystemError);
	}

	Assets.reserve(ScannedAssets.size());
	for (auto& Entry : ScannedAssets)
	{
		FAssetData& Asset = Entry.second;
		FString FolderPath = Asset.FolderPath;
		while (FolderPath != "/")
		{
			FolderSet.emplace(FolderPath);
			FolderPath = FPaths::GetPath(FolderPath);
		}
		Assets.push_back(std::move(Asset));
	}

	std::sort(Assets.begin(), Assets.end(), [](const FAssetData& Left, const FAssetData& Right)
	{
		return Left.AssetPath < Right.AssetPath;
	});
	AssetIdIndices.reserve(Assets.size());
	AssetPathIndices.reserve(Assets.size());
	for (SIZE_T AssetIndex = 0; AssetIndex < Assets.size(); ++AssetIndex)
	{
		FAssetData& Asset = Assets[AssetIndex];
		AssetPathIndices.emplace(Asset.AssetPath, AssetIndex);
		if (!Asset.AssetId.IsValid())
		{
			continue;
		}
		const auto [It, bInserted] = AssetIdIndices.emplace(Asset.AssetId, AssetIndex);
		if (!bInserted)
		{
			KE_LOG(LogAssetRegistry, Error, "중복 Asset ID를 발견했다. AssetId={}, FirstPath={}, DuplicatePath={}",
				Asset.AssetId.ToString(), Assets[It->second].AssetPath, Asset.AssetPath);
			Asset.AssetId = {};
			Asset.Type = EAssetType::Unknown;
		}
	}

	Folders.assign(FolderSet.begin(), FolderSet.end());
	std::sort(Folders.begin(), Folders.end());
	KE_LOG(LogAssetRegistry, Log, "Content 스캔 완료. Assets={}, Folders={}", Assets.size(), Folders.size());
}

void FAssetRegistry::Reset()
{
	Assets.clear();
	Folders.clear();
	AssetIdIndices.clear();
	AssetPathIndices.clear();
}

const FAssetData* FAssetRegistry::FindAsset(const FAssetId& AssetId) const
{
	const auto It = AssetIdIndices.find(AssetId);
	return It != AssetIdIndices.end() ? &Assets[It->second] : nullptr;
}

const FAssetData* FAssetRegistry::FindAsset(const FString& AssetPath) const
{
	const auto It = AssetPathIndices.find(AssetPath);
	return It != AssetPathIndices.end() ? &Assets[It->second] : nullptr;
}
