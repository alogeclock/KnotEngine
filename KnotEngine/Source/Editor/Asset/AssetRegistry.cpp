#include "Asset/AssetRegistry.h"

#include "Core/IO/Paths.h"
#include "Core/Log.h"

#include <algorithm>
#include <cctype>
#include <system_error>

// 확장자를 제외한 Content 상대 경로를 논리 Asset 경로로 변환한다.
static FString MakeAssetPath(const std::filesystem::path& RelativeFilePath)
{
	std::filesystem::path RelativeAssetPath = RelativeFilePath;
	RelativeAssetPath.replace_extension();
	return "/" + FPaths::ToUtf8(RelativeAssetPath.generic_wstring());
}

// Content의 원본과 바이너리를 다시 찾아 Asset 목록과 경로 인덱스를 교체한다.
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
						Asset.Type = EAssetType::StaticMesh;
					}
					else
					{
						Asset.BinaryFilePath = Entry.path();
						Asset.Type = EAssetType::StaticMesh;
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
	AssetIndices.reserve(Assets.size());
	for (SIZE_T AssetIndex = 0; AssetIndex < Assets.size(); ++AssetIndex)
	{
		AssetIndices.emplace(Assets[AssetIndex].AssetPath, AssetIndex);
	}

	Folders.assign(FolderSet.begin(), FolderSet.end());
	std::sort(Folders.begin(), Folders.end());
	KE_LOG(LogAssetRegistry, Log, "Content 스캔 완료. Assets={}, Folders={}", Assets.size(), Folders.size());
}

// 스캔 결과와 경로 인덱스를 비운다.
void FAssetRegistry::Reset()
{
	Assets.clear();
	Folders.clear();
	AssetIndices.clear();
}

// 논리 경로에 대응하는 스캔 결과를 찾는다.
const FAssetData* FAssetRegistry::FindAsset(const FString& AssetPath) const
{
	const auto It = AssetIndices.find(AssetPath);
	return It != AssetIndices.end() ? &Assets[It->second] : nullptr;
}
