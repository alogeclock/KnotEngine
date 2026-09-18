#pragma once

#include "Asset/AssetRegistry.h"
#include "Render/RHI/RenderTypes.h"

#include <array>
#include <string_view>

class IImGuiRenderBackend;
class IRenderDevice;
class FAssetImportManager;
struct ImVec2;

// Asset Registry의 폴더와 Asset 메타데이터를 탐색하고 Content 파일 작업을 제공하는 Editor 창이다.
class FContentPanel final
{
public:
	FContentPanel(FAssetRegistry& InAssetRegistry, FAssetImportManager& InAssetImportManager, IRenderDevice& InRenderDevice, IImGuiRenderBackend& InRenderBackend);

	void Startup();
	void Draw();
	void Shutdown();

private:
	static constexpr uint32 ContentIconSize = 128;
	static const uint32 DropHighlightColor;

	enum class EItemType : uint8
	{
		None,
		Asset,
		Folder,
	};

	static bool ContainsText(std::string_view Text, std::string_view SearchText);
	static bool DecodeIcon(const std::filesystem::path& FilePath, TArray<uint8>& Pixels);
	static FString MakeTileLabel(const FString& Label, float Width);

	void DrawFolderPane();
	void DrawFolderTree(const FString& FolderPath);
	void DrawContentPane();
	void DrawToolbar();
	void DrawBreadcrumbs(float Width);
	void DrawContentTiles();
	void DrawFolderTile(const FString& FolderPath, float TileWidth, float TileHeight);
	void DrawAssetTile(const FAssetData& Asset, float TileWidth, float TileHeight);
	void DrawContextMenu();

	void Navigate(const FString& FolderPath, bool bRecordHistory = true);
	void OpenContextMenu(const FString& FolderPath, const FString& AssetPath);
	void Refresh();

	void CreateFolder(const FString& ParentFolderPath);
	void OpenInFileExplorer() const;
	void ImportAsset();

	bool RenameItem();
	void CopyItem();
	void PasteItem(const FString& FolderPath);
	void DeleteItem();

	void MoveItem(EItemType ItemType, const FString& SourcePath, const FString& DestinationFolderPath);
	void DropItem(const FString& FolderPath, const ImVec2& Minimum, const ImVec2& Maximum);

	bool IsFolderVisible(const FString& FolderPath) const;
	bool IsFolderSearchMatch(const FString& FolderPath) const;
	bool IsAssetSearchMatch(const FAssetData& Asset) const;
	bool IsDirectChildFolder(const FString& FolderPath, const FString& ParentPath) const;

	FString GetFolderName(const FString& FolderPath) const;
	std::filesystem::path MakeUniquePath(const std::filesystem::path& DesiredPath, bool bDirectory) const;

	FAssetRegistry& AssetRegistry;
	FAssetImportManager& AssetImportManager;
	IRenderDevice& RenderDevice;
	IImGuiRenderBackend& RenderBackend;

	FTextureHandle FolderIcon;
	FTextureHandle FileIcon;

	FString SelectedFolderPath = "/";
	FString SelectedAssetPath;

	FString ContextFolderPath = "/";
	FString ContextAssetPath;

	EItemType CopiedItemType = EItemType::None;
	FString CopiedAssetPath;
	FString CopiedFolderPath;

	TArray<FString> NavigationHistory = { "/" };
	SIZE_T NavigationIndex = 0;

	TStaticArray<char, 128> FolderSearchText = {};
	TStaticArray<char, 128> AssetSearchText = {};
	TStaticArray<char, 128> RenameText = {};

	bool bOpenContextMenu = false;
	bool bOpenRenamePopup = false;
	bool bOpenDeleteConfirmation = false;
	bool bRefreshRequested = false;
};
