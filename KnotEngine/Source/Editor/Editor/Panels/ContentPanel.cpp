#include "Editor/Panels/ContentPanel.h"

#include <algorithm>
#include <cctype>
#include <imgui.h>

// Asset 타입을 Content 목록에 표시할 문자열로 변환한다.
static const char* GetAssetTypeText(EAssetType Type)
{
	switch (Type)
	{
	case EAssetType::StaticMesh:
		return "Static Mesh";
	default:
		return "Unknown";
	}
}

// ASCII 검색어가 Asset 이름 또는 논리 경로에 포함되는지 확인한다.
static bool ContainsSearchText(const FAssetData& Asset, const char* SearchText)
{
	if (!SearchText[0])
	{
		return true;
	}

	FString LowerSearch = SearchText;
	FString LowerName = Asset.Name;
	FString LowerPath = Asset.AssetPath;
	const auto ToLower = [](unsigned char Character) { return static_cast<char>(std::tolower(Character)); };
	std::transform(LowerSearch.begin(), LowerSearch.end(), LowerSearch.begin(), ToLower);
	std::transform(LowerName.begin(), LowerName.end(), LowerName.begin(), ToLower);
	std::transform(LowerPath.begin(), LowerPath.end(), LowerPath.begin(), ToLower);
	return LowerName.find(LowerSearch) != FString::npos || LowerPath.find(LowerSearch) != FString::npos;
}

// 선택 폴더 아래에 Asset이 직접 속하는지, 검색 중에는 하위 폴더까지 포함하는지 확인한다.
static bool IsAssetInFolder(const FAssetData& Asset, const FString& FolderPath, bool bRecursive)
{
	if (!bRecursive)
	{
		return Asset.FolderPath == FolderPath;
	}
	if (FolderPath == "/")
	{
		return true;
	}
	return Asset.FolderPath == FolderPath || Asset.FolderPath.starts_with(FolderPath + "/");
}

FContentPanel::FContentPanel(FAssetRegistry& InAssetRegistry)
	: AssetRegistry(InAssetRegistry)
{
}

// Content Toolbar, 폴더 목록과 Asset 표를 구성한다.
void FContentPanel::Draw()
{
	if (!ImGui::Begin("Content"))
	{
		ImGui::End();
		return;
	}

	if (ImGui::Button("Refresh"))
	{
		AssetRegistry.Scan();
		if (!SelectedAssetPath.empty() && !AssetRegistry.FindAsset(SelectedAssetPath))
		{
			SelectedAssetPath.clear();
		}
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth(std::max(120.0f, ImGui::GetContentRegionAvail().x));
	ImGui::InputTextWithHint("##ContentSearch", "Search assets...", SearchText.data(), SearchText.size());
	ImGui::Separator();

	const float FolderWidth = std::clamp(ImGui::GetContentRegionAvail().x * 0.24f, 130.0f, 240.0f);
	if (ImGui::BeginChild("ContentFolders", ImVec2(FolderWidth, -ImGui::GetFrameHeightWithSpacing()), ImGuiChildFlags_Borders))
	{
		DrawFolderList();
	}
	ImGui::EndChild();
	ImGui::SameLine();
	if (ImGui::BeginChild("ContentAssets", ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing()), ImGuiChildFlags_Borders))
	{
		DrawAssetList();
	}
	ImGui::EndChild();

	if (SelectedAssetPath.empty())
	{
		ImGui::TextDisabled("No asset selected");
	}
	else
	{
		ImGui::TextUnformatted(SelectedAssetPath.c_str());
	}
	ImGui::End();
}

// 스캔된 폴더를 계층 깊이에 맞춰 왼쪽 목록에 표시한다.
void FContentPanel::DrawFolderList()
{
	for (const FString& FolderPath : AssetRegistry.GetFolders())
	{
		const SIZE_T Depth = FolderPath == "/" ? 0 : static_cast<SIZE_T>(std::count(FolderPath.begin(), FolderPath.end(), '/'));
		const SIZE_T Separator = FolderPath.find_last_of('/');
		const FString FolderName = FolderPath == "/" ? "Contents" : FolderPath.substr(Separator + 1);
		ImGui::PushID(FolderPath.c_str());
		ImGui::Indent(static_cast<float>(Depth) * 12.0f);
		if (ImGui::Selectable(FolderName.c_str(), SelectedFolderPath == FolderPath))
		{
			SelectedFolderPath = FolderPath;
		}
		ImGui::Unindent(static_cast<float>(Depth) * 12.0f);
		ImGui::PopID();
	}
}

// 선택 폴더와 검색어에 맞는 Asset을 표로 표시한다.
void FContentPanel::DrawAssetList()
{
	constexpr ImGuiTableFlags TableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
		ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;
	if (!ImGui::BeginTable("ContentAssetsTable", 3, TableFlags))
	{
		return;
	}

	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.50f);
	ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 0.30f);
	ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthStretch, 0.20f);
	ImGui::TableHeadersRow();

	const bool bSearching = SearchText[0] != '\0';
	for (const FAssetData& Asset : AssetRegistry.GetAssets())
	{
		if (!IsAssetInFolder(Asset, SelectedFolderPath, bSearching) || !ContainsSearchText(Asset, SearchText.data()))
		{
			continue;
		}

		ImGui::PushID(Asset.AssetPath.c_str());
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		if (ImGui::Selectable(Asset.Name.c_str(), SelectedAssetPath == Asset.AssetPath, ImGuiSelectableFlags_SpanAllColumns))
		{
			SelectedAssetPath = Asset.AssetPath;
		}
		ImGui::TableSetColumnIndex(1);
		ImGui::TextUnformatted(GetAssetTypeText(Asset.Type));
		ImGui::TableSetColumnIndex(2);
		ImGui::TextUnformatted(Asset.HasSourceFile() ? "Blender" : "Binary");
		ImGui::PopID();
	}
	ImGui::EndTable();
}
