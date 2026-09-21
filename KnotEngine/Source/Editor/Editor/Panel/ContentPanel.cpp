#include "Editor/Panel/ContentPanel.h"

#include "Asset/AssetImportManager.h"
#include "Core/IO/Paths.h"
#include "Core/Log.h"
#include "Render/RenderSystem.h"

#include <Windows.h>
#include <Shellapi.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <imgui.h>
#include <objbase.h>
#include <system_error>
#include <wincodec.h>
#include <wrl/client.h>

const uint32 FContentPanel::DropHighlightColor = IM_COL32(43, 188, 255, 255);

// 대소문자를 구분하지 않고 문자열에 검색어가 포함되는지 확인한다.
bool FContentPanel::ContainsText(std::string_view Text, std::string_view SearchText)
{
	if (SearchText.empty())
	{
		return true;
	}

	const auto EqualIgnoreCase = [](char Left, char Right)
	{
		return std::tolower(static_cast<unsigned char>(Left)) == std::tolower(static_cast<unsigned char>(Right));
	};
	return std::search(Text.begin(), Text.end(), SearchText.begin(), SearchText.end(), EqualIgnoreCase) != Text.end();
}

// PNG 파일을 Content Browser 썸네일 크기의 RGBA Pixel 배열로 디코딩한다.
bool FContentPanel::DecodeIcon(const std::filesystem::path& FilePath, TArray<uint8>& Pixels)
{
	const HRESULT InitializeResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	const bool bUninitialize = SUCCEEDED(InitializeResult);
	bool bDecoded = false;
	{
		Microsoft::WRL::ComPtr<IWICImagingFactory> Factory;
		Microsoft::WRL::ComPtr<IWICBitmapDecoder> Decoder;
		Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> Frame;
		Microsoft::WRL::ComPtr<IWICBitmapScaler> Scaler;
		Microsoft::WRL::ComPtr<IWICFormatConverter> Converter;
		if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&Factory))) &&
		    SUCCEEDED(Factory->CreateDecoderFromFilename(FilePath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &Decoder)) &&
		    SUCCEEDED(Decoder->GetFrame(0, &Frame)) && SUCCEEDED(Factory->CreateBitmapScaler(&Scaler)) &&
		    SUCCEEDED(Scaler->Initialize(Frame.Get(), ContentIconSize, ContentIconSize, WICBitmapInterpolationModeFant)) &&
		    SUCCEEDED(Factory->CreateFormatConverter(&Converter)) &&
		    SUCCEEDED(Converter->Initialize(Scaler.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)))
		{
			Pixels.resize(static_cast<SIZE_T>(ContentIconSize) * ContentIconSize * 4);
			bDecoded = SUCCEEDED(Converter->CopyPixels(nullptr, ContentIconSize * 4, static_cast<UINT>(Pixels.size()), Pixels.data()));
		}
	}
	if (bUninitialize)
	{
		CoUninitialize();
	}
	return bDecoded;
}

// 복제된 .kasset이 원본과 충돌하지 않도록 컨테이너 헤더에 새로운 영속 ID를 기록한다.
bool FContentPanel::RegenerateAssetId(const std::filesystem::path& FilePath)
{
	std::fstream Stream(FilePath, std::ios::binary | std::ios::in | std::ios::out);
	FAssetFileHeader Header = {};
	if (!Stream.read(reinterpret_cast<char*>(&Header), sizeof(Header)) ||
		std::memcmp(Header.Magic, FAssetFileHeader::MagicValue, sizeof(Header.Magic)) != 0 ||
		Header.ContainerVersion != FAssetFileHeader::CurrentVersion)
	{
		return false;
	}
	Header.AssetId = FAssetId::New();
	Stream.seekp(0, std::ios::beg);
	return static_cast<bool>(Stream.write(reinterpret_cast<const char*>(&Header), sizeof(Header)));
}

// 긴 Tile 이름을 폭에 맞게 생략 부호가 붙은 문자열로 줄인다.
FString FContentPanel::MakeTileLabel(const FString& Label, float Width)
{
	if (ImGui::CalcTextSize(Label.c_str()).x <= Width)
	{
		return Label;
	}

	FString Result = Label;
	while (!Result.empty() && ImGui::CalcTextSize((Result + "...").c_str()).x > Width)
	{
		Result.pop_back();
	}
	return Result + "...";
}

FContentPanel::FContentPanel(
	FAssetRegistry& InAssetRegistry,
	FAssetImportManager& InAssetImportManager,
	FRenderSystem& InRenderSystem)
    : AssetRegistry(InAssetRegistry), AssetImportManager(InAssetImportManager), RenderSystem(InRenderSystem)
{
}

// 기본 Folder와 File PNG를 GPU 썸네일 Texture로 생성한다.
void FContentPanel::Startup()
{
	TArray<uint8> Pixels;
	FTextureDesc IconDesc;
	IconDesc.Width = ContentIconSize;
	IconDesc.Height = ContentIconSize;
	IconDesc.Format = ETextureFormat::RGBA8UNorm;
	const std::filesystem::path IconDirectory = std::filesystem::path(FPaths::ContentDir()) / L"Engine/Icon";
	if (DecodeIcon(IconDirectory / L"ContentFolder.png", Pixels))
	{
		const FTextureSubresourceData IconData = { Pixels, ContentIconSize * 4, static_cast<uint32>(Pixels.size()) };
		FolderIcon = RenderSystem.CreateTexture(IconDesc, std::span<const FTextureSubresourceData>(&IconData, 1));
		FolderIconTextureId = RenderSystem.GetImGuiTextureID(FolderIcon);
	}
	else
	{
		KE_LOG(LogContentPanel, Warning, "Content Folder Icon을 불러오지 못했다.");
	}
	if (DecodeIcon(IconDirectory / L"ContentFile.png", Pixels))
	{
		const FTextureSubresourceData IconData = { Pixels, ContentIconSize * 4, static_cast<uint32>(Pixels.size()) };
		FileIcon = RenderSystem.CreateTexture(IconDesc, std::span<const FTextureSubresourceData>(&IconData, 1));
		FileIconTextureId = RenderSystem.GetImGuiTextureID(FileIcon);
	}
	else
	{
		KE_LOG(LogContentPanel, Warning, "Content File Icon을 불러오지 못했다.");
	}
}

// Folder와 Content Tile 영역을 좌우로 배치한다.
void FContentPanel::Draw()
{
	const bool bContentVisible = ImGui::Begin("Content");
	if (bContentVisible)
	{
		const float FolderWidth = std::clamp(ImGui::GetContentRegionAvail().x * 0.22f, 170.0f, 280.0f);
		if (ImGui::BeginChild("ContentFolders", ImVec2(FolderWidth, 0.0f), ImGuiChildFlags_Borders))
		{
			DrawFolderPane();
		}
		ImGui::EndChild();
		ImGui::SameLine();
		if (ImGui::BeginChild("ContentItems", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders))
		{
			DrawContentPane();
		}
		ImGui::EndChild();

		if (bOpenContextMenu)
		{
			ImGui::OpenPopup("ContentContext");
			bOpenContextMenu = false;
		}
		DrawContextMenu();
	}
	ImGui::End();
	DrawImportOptions();

	if (bRefreshRequested)
	{
		bRefreshRequested = false;
		Refresh();
	}
}

// Asset Tile 더블 클릭으로 발생한 편집기 열기 요청을 Consume 방식으로 한 번만 반환한다.
std::optional<FAssetId> FContentPanel::OpenAssetEditor()
{
	std::optional<FAssetId> Request = OpenAssetRequest;
	OpenAssetRequest.reset();
	return Request;
}

// GLB Import 설정을 조정하고 비동기 Import Queue에 요청을 등록하는 Modal을 표시한다.
void FContentPanel::DrawImportOptions()
{
	static constexpr const char* PopupTitle = "GLB Import Options";
	if (bOpenImportOptions)
	{
		ImGui::OpenPopup(PopupTitle);
		bOpenImportOptions = false;
	}

	if (!ImGui::BeginPopupModal(PopupTitle, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		return;
	}

	ImGui::TextUnformatted(FPaths::ToUtf8(PendingImportSourceFilePath.filename().wstring()).c_str());
	ImGui::TextDisabled("Source: %s", FPaths::ToUtf8(PendingImportSourceFilePath.generic_wstring()).c_str());
	ImGui::TextDisabled("Destination: %s", PendingImportDestinationAssetPath.c_str());
	ImGui::Separator();

	if (ImGui::CollapsingHeader("Static Mesh", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Checkbox("Combine Meshes", &PendingImportOptions.bCombineMeshes);
		ImGui::TextDisabled("Bake scene node transforms and combine all mesh nodes into one Static Mesh.");
		ImGui::SetNextItemWidth(180.0f);
		ImGui::DragFloat("Uniform Scale", &PendingImportOptions.UniformScale, 0.01f, 0.0001f, 1000.0f, "%.4f");
		ImGui::TextDisabled("1.0 converts glTF meters to Knot Engine centimeters.");

		ImGui::Spacing();
		ImGui::TextUnformatted("Generated LODs");
		for (SIZE_T LODIndex = 0; LODIndex < PendingImportOptions.LODTriangleRatios.size(); ++LODIndex)
		{
			float Percentage = PendingImportOptions.LODTriangleRatios[LODIndex] * 100.0f;
			const FString Label = "LOD " + std::to_string(LODIndex + 1) + " Triangle Ratio";
			ImGui::SetNextItemWidth(180.0f);
			if (ImGui::DragFloat(Label.c_str(), &Percentage, 1.0f, 1.0f, 99.0f, "%.0f%%", ImGuiSliderFlags_AlwaysClamp))
			{
				PendingImportOptions.LODTriangleRatios[LODIndex] = Percentage * 0.01f;
			}
		}
		ImGui::BeginDisabled(PendingImportOptions.LODTriangleRatios.size() >= FGLBImportOptions::MaxGeneratedLODCount);
		if (ImGui::SmallButton("Add LOD"))
		{
			const float PreviousRatio = PendingImportOptions.LODTriangleRatios.empty() ? 1.0f : PendingImportOptions.LODTriangleRatios.back();
			PendingImportOptions.LODTriangleRatios.push_back(std::max(0.01f, PreviousRatio * 0.5f));
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(PendingImportOptions.LODTriangleRatios.empty());
		if (ImGui::SmallButton("Remove Last LOD"))
		{
			PendingImportOptions.LODTriangleRatios.pop_back();
		}
		ImGui::EndDisabled();
		ImGui::TextDisabled("Each ratio is measured against the original LOD 0 triangle count.");
	}

	const bool bScaleValid = std::isfinite(PendingImportOptions.UniformScale) && PendingImportOptions.UniformScale > 0.0f;
	bool bLODRatiosValid = PendingImportOptions.LODTriangleRatios.size() <= FGLBImportOptions::MaxGeneratedLODCount;
	float PreviousLODRatio = 1.0f;
	for (const float LODRatio : PendingImportOptions.LODTriangleRatios)
	{
		if (!std::isfinite(LODRatio) || LODRatio <= 0.0f || LODRatio >= PreviousLODRatio)
		{
			bLODRatiosValid = false;
			break;
		}
		PreviousLODRatio = LODRatio;
	}
	if (!bScaleValid)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.25f, 1.0f), "Uniform Scale must be greater than zero.");
	}
	if (!bLODRatiosValid)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.25f, 1.0f), "Each LOD ratio must be lower than the previous LOD.");
	}

	ImGui::Separator();
	ImGui::BeginDisabled(!bScaleValid || !bLODRatiosValid || AssetImportManager.HasActiveImports());
	if (ImGui::Button("Import"))
	{
		if (AssetImportManager.EnqueueGLB(PendingImportSourceFilePath, PendingImportDestinationAssetPath, PendingImportOptions))
		{
			PendingImportSourceFilePath.clear();
			PendingImportDestinationAssetPath.clear();
			ImGui::CloseCurrentPopup();
		}
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button("Cancel"))
	{
		PendingImportSourceFilePath.clear();
		PendingImportDestinationAssetPath.clear();
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}

// Content Browser가 소유한 썸네일 Texture를 해제한다.
void FContentPanel::Shutdown()
{
	RenderSystem.DestroyTexture(FileIcon);
	RenderSystem.DestroyTexture(FolderIcon);
	FileIconTextureId = {};
	FolderIconTextureId = {};
}

// 폴더 검색창과 계층 트리를 표시한다.
void FContentPanel::DrawFolderPane()
{
	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputTextWithHint("##FolderSearch", "Search folders...", FolderSearchText.data(), FolderSearchText.size());
	ImGui::Separator();
	if (ImGui::BeginChild("FolderTree", ImVec2(0.0f, 0.0f)))
	{
		DrawFolderTree("/");
		if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !ImGui::IsAnyItemHovered())
		{
			OpenContextMenu(SelectedFolderPath, {});
		}
	}
	ImGui::EndChild();
}

// Flat Registry 폴더 목록에서 직접 자식만 찾아 재귀적인 Folder Tree를 그린다.
void FContentPanel::DrawFolderTree(const FString& FolderPath)
{
	if (!IsFolderVisible(FolderPath))
	{
		return;
	}

	bool bHasChildren = false;
	for (const FString& Candidate : AssetRegistry.GetFolders())
	{
		if (IsDirectChildFolder(Candidate, FolderPath) && IsFolderVisible(Candidate))
		{
			bHasChildren = true;
			break;
		}
	}

	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
	if (!bHasChildren)
	{
		Flags |= ImGuiTreeNodeFlags_Leaf;
	}
	if (SelectedFolderPath == FolderPath)
	{
		Flags |= ImGuiTreeNodeFlags_Selected;
	}
	if (FolderPath == "/")
	{
		Flags |= ImGuiTreeNodeFlags_DefaultOpen;
	}
	if (FolderSearchText[0])
	{
		ImGui::SetNextItemOpen(true);
	}

	ImGui::PushID(FolderPath.c_str());
	const bool bOpen = ImGui::TreeNodeEx("##Folder", Flags, "%s", GetFolderName(FolderPath).c_str());
	const ImVec2 ItemMinimum = ImGui::GetItemRectMin();
	const ImVec2 ItemMaximum = ImGui::GetItemRectMax();
	if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
	{
		Navigate(FolderPath);
	}
	if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
	{
		OpenContextMenu(FolderPath, {});
	}
	if (FolderPath != "/" && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
	{
		ImGui::SetDragDropPayload("CONTENT_FOLDER", FolderPath.c_str(), FolderPath.size() + 1);
		ImGui::TextUnformatted(GetFolderName(FolderPath).c_str());
		ImGui::EndDragDropSource();
	}
	DropItem(FolderPath, ItemMinimum, ItemMaximum);

	if (bOpen)
	{
		for (const FString& Candidate : AssetRegistry.GetFolders())
		{
			if (IsDirectChildFolder(Candidate, FolderPath))
			{
				DrawFolderTree(Candidate);
			}
		}
		ImGui::TreePop();
	}
	ImGui::PopID();
}

// 탐색 Toolbar, Content 검색창과 Tile 목록을 표시한다.
void FContentPanel::DrawContentPane()
{
	DrawToolbar();
	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputTextWithHint("##AssetSearch", "Search files and folders...", AssetSearchText.data(), AssetSearchText.size());
	ImGui::Separator();
	if (ImGui::BeginChild("ContentTileScroll", ImVec2(0.0f, 0.0f)))
	{
		DrawContentTiles();
		if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !ImGui::IsAnyItemHovered())
		{
			OpenContextMenu(SelectedFolderPath, {});
		}
	}
	ImGui::EndChild();
}

// 뒤로, 앞으로, 현재 경로, Refresh 순서로 Content Toolbar를 구성한다.
void FContentPanel::DrawToolbar()
{
	const float ToolbarHeight = ImGui::GetFrameHeight();
	const auto DrawNavigationButton = [ToolbarHeight](const char* Id, ImGuiDir Direction, bool bEnabled)
	{
		ImGui::BeginDisabled(!bEnabled);
		const bool bClicked = ImGui::Button(Id, ImVec2(ToolbarHeight, ToolbarHeight));
		ImGui::EndDisabled();

		const ImVec2 Minimum = ImGui::GetItemRectMin();
		const ImVec2 Maximum = ImGui::GetItemRectMax();
		const ImVec2 Center((Minimum.x + Maximum.x) * 0.5f, (Minimum.y + Maximum.y) * 0.5f);
		const float Radius = ToolbarHeight * 0.18f;
		const ImU32 Color = ImGui::GetColorU32(bEnabled ? ImGuiCol_Text : ImGuiCol_TextDisabled);
		if (Direction == ImGuiDir_Left)
		{
			ImGui::GetWindowDrawList()->AddTriangleFilled(ImVec2(Center.x - Radius, Center.y), ImVec2(Center.x + Radius, Center.y - Radius),
			                                              ImVec2(Center.x + Radius, Center.y + Radius), Color);
		}
		else
		{
			ImGui::GetWindowDrawList()->AddTriangleFilled(ImVec2(Center.x + Radius, Center.y), ImVec2(Center.x - Radius, Center.y - Radius),
			                                              ImVec2(Center.x - Radius, Center.y + Radius), Color);
		}
		return bClicked;
	};

	if (DrawNavigationButton("##ContentBack", ImGuiDir_Left, NavigationIndex > 0))
	{
		--NavigationIndex;
		Navigate(NavigationHistory[NavigationIndex], false);
	}
	ImGui::SameLine();
	if (DrawNavigationButton("##ContentForward", ImGuiDir_Right, NavigationIndex + 1 < NavigationHistory.size()))
	{
		++NavigationIndex;
		Navigate(NavigationHistory[NavigationIndex], false);
	}
	ImGui::SameLine();

	const float RefreshWidth = ImGui::CalcTextSize("Refresh").x + ImGui::GetStyle().FramePadding.x * 2.0f;
	const float BreadcrumbWidth = std::max(80.0f, ImGui::GetContentRegionAvail().x - RefreshWidth - ImGui::GetStyle().ItemSpacing.x);
	DrawBreadcrumbs(BreadcrumbWidth);
	ImGui::SameLine();
	if (ImGui::Button("Refresh", ImVec2(ImGui::GetContentRegionAvail().x, ToolbarHeight)))
	{
		Refresh();
	}
}

// 현재 Folder 경로를 클릭 가능한 Breadcrumb로 표시한다.
void FContentPanel::DrawBreadcrumbs(float Width)
{
	const float BreadcrumbHeight = ImGui::GetFrameHeight();
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5.0f, 0.0f));
	const bool bVisible = ImGui::BeginChild("ContentBreadcrumbs", ImVec2(Width, BreadcrumbHeight), ImGuiChildFlags_Borders,
	                                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	ImGui::PopStyleVar();
	if (!bVisible)
	{
		ImGui::EndChild();
		return;
	}

	ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 0.82f);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 0.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 1.0f);
	ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Header));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));

	const float SegmentHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
	const float VerticalOffset = std::max(0.0f, (ImGui::GetContentRegionAvail().y - SegmentHeight) * 0.5f);
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + VerticalOffset);
	if (ImGui::Button("Content"))
	{
		Navigate("/");
	}

	const FString FolderPath = SelectedFolderPath;
	FString CurrentPath;
	SIZE_T SegmentStart = 1;
	while (SegmentStart < FolderPath.size())
	{
		const SIZE_T Separator = FolderPath.find('/', SegmentStart);
		const FString Segment = FolderPath.substr(SegmentStart, Separator - SegmentStart);
		CurrentPath += "/" + Segment;
		ImGui::SameLine();
		ImGui::AlignTextToFramePadding();
		ImGui::TextDisabled(">");
		ImGui::SameLine();
		ImGui::PushID(CurrentPath.c_str());
		if (ImGui::Button(Segment.c_str()))
		{
			Navigate(CurrentPath);
		}
		ImGui::PopID();
		if (Separator == FString::npos)
		{
			break;
		}
		SegmentStart = Separator + 1;
	}
	ImGui::PopStyleColor(3);
	ImGui::PopStyleVar(3);
	ImGui::PopFont();
	ImGui::EndChild();
}

// 선택 Folder의 직접 자식과 검색 결과를 반응형 Tile Grid로 표시한다.
void FContentPanel::DrawContentTiles()
{
	constexpr float TileWidth = 88.0f;
	constexpr float TileHeight = 112.0f;
	const float Spacing = ImGui::GetStyle().ItemSpacing.x;
	const int32 ColumnCount = std::max(1, static_cast<int32>((ImGui::GetContentRegionAvail().x + Spacing) / (TileWidth + Spacing)));
	if (!ImGui::BeginTable("ContentTileGrid", ColumnCount, ImGuiTableFlags_SizingFixedFit))
	{
		return;
	}

	const bool bSearching = AssetSearchText[0] != '\0';
	for (const FString& FolderPath : AssetRegistry.GetFolders())
	{
		const bool bInScope = bSearching
		                          ? FolderPath != "/" && FPaths::IsInside(FolderPath, SelectedFolderPath)
		                          : IsDirectChildFolder(FolderPath, SelectedFolderPath);
		if (bInScope && ContainsText(GetFolderName(FolderPath), AssetSearchText.data()))
		{
			ImGui::TableNextColumn();
			DrawFolderTile(FolderPath, TileWidth, TileHeight);
		}
	}
	for (const FAssetData& Asset : AssetRegistry.GetAssets())
	{
		const bool bInScope = bSearching ? FPaths::IsInside(Asset.FolderPath, SelectedFolderPath) : Asset.FolderPath == SelectedFolderPath;
		if (bInScope && IsAssetSearchMatch(Asset))
		{
			ImGui::TableNextColumn();
			DrawAssetTile(Asset, TileWidth, TileHeight);
		}
	}
	ImGui::EndTable();
}

// Folder 썸네일 Tile과 Drop Target을 표시한다.
void FContentPanel::DrawFolderTile(const FString& FolderPath, float TileWidth, float TileHeight)
{
	ImGui::PushID(FolderPath.c_str());
	const ImVec2 TileMin = ImGui::GetCursorScreenPos();
	ImGui::InvisibleButton("##FolderTile", ImVec2(TileWidth, TileHeight));
	const ImVec2 TileMax = ImGui::GetItemRectMax();
	const bool bHovered = ImGui::IsItemHovered();
	const ImU32 Background = ImGui::GetColorU32(bHovered ? ImGuiCol_HeaderHovered : ImGuiCol_FrameBg);
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->AddRectFilled(TileMin, TileMax, Background, 5.0f);
	DrawList->AddRect(TileMin, TileMax, ImGui::GetColorU32(ImGuiCol_Border), 5.0f);

	const ImVec2 IconMin(TileMin.x + 20.0f, TileMin.y + 8.0f);
	const ImVec2 IconMax(IconMin.x + 48.0f, IconMin.y + 48.0f);
	if (FolderIcon.IsValid())
	{
		DrawList->AddImage(ImTextureRef(FolderIconTextureId), IconMin, IconMax);
	}
	const FString FolderName = GetFolderName(FolderPath);
	ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 0.90f);
	ImFont* TileFont = ImGui::GetFont();
	const float TileFontSize = ImGui::GetFontSize();
	const FString Label = MakeTileLabel(FolderName, TileWidth - 10.0f);
	const ImVec2 LabelSize = ImGui::CalcTextSize(Label.c_str());
	const ImVec2 TypeSize = ImGui::CalcTextSize("Folder");
	ImGui::PopFont();
	DrawList->AddText(TileFont, TileFontSize, ImVec2(TileMin.x + (TileWidth - LabelSize.x) * 0.5f, TileMin.y + 63.0f), ImGui::GetColorU32(ImGuiCol_Text), Label.c_str());
	DrawList->AddText(TileFont, TileFontSize, ImVec2(TileMin.x + (TileWidth - TypeSize.x) * 0.5f, TileMin.y + 90.0f), ImGui::GetColorU32(ImGuiCol_TextDisabled), "Folder");

	if (bHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
	{
		Navigate(FolderPath);
	}
	if (bHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		OpenContextMenu(FolderPath, {});
	}
	if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
	{
		ImGui::SetDragDropPayload("CONTENT_FOLDER", FolderPath.c_str(), FolderPath.size() + 1);
		ImGui::TextUnformatted(FolderName.c_str());
		ImGui::EndDragDropSource();
	}
	DropItem(FolderPath, TileMin, TileMax);
	ImGui::PopID();
}

// Asset 썸네일 Tile을 표시하고 이동용 Drag Payload를 시작한다.
void FContentPanel::DrawAssetTile(const FAssetData& Asset, float TileWidth, float TileHeight)
{
	const char* TypeLabel = "GLB Source";
	if (Asset.Type == EAssetType::StaticMesh)
	{
		TypeLabel = "Static Mesh";
	}
	else if (Asset.Type == EAssetType::Material)
	{
		TypeLabel = "Material";
	}
	else if (Asset.Type == EAssetType::Texture2D)
	{
		TypeLabel = "Texture 2D";
	}
	ImGui::PushID(Asset.AssetPath.c_str());
	const ImVec2 TileMinimum = ImGui::GetCursorScreenPos();
	ImGui::InvisibleButton("##AssetTile", ImVec2(TileWidth, TileHeight));
	const ImVec2 TileMaximum = ImGui::GetItemRectMax();
	const bool bHovered = ImGui::IsItemHovered();
	const bool bSelected = SelectedAssetPath == Asset.AssetPath;
	const ImU32 Background = ImGui::GetColorU32(bSelected ? ImGuiCol_Header : bHovered ? ImGuiCol_HeaderHovered
	                                                                                   : ImGuiCol_FrameBg);
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->AddRectFilled(TileMinimum, TileMaximum, Background, 5.0f);
	DrawList->AddRect(TileMinimum, TileMaximum, ImGui::GetColorU32(ImGuiCol_Border), 5.0f);

	const ImVec2 IconMinimum(TileMinimum.x + 20.0f, TileMinimum.y + 8.0f);
	const ImVec2 IconMaximum(IconMinimum.x + 48.0f, IconMinimum.y + 48.0f);
	if (FileIcon.IsValid())
	{
		DrawList->AddImage(ImTextureRef(FileIconTextureId), IconMinimum, IconMaximum);
	}
	ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 0.82f);
	ImFont* TileFont = ImGui::GetFont();
	const float TileFontSize = ImGui::GetFontSize();
	const FString Label = MakeTileLabel(Asset.Name, TileWidth - 10.0f);
	const ImVec2 LabelSize = ImGui::CalcTextSize(Label.c_str());
	const ImVec2 TypeSize = ImGui::CalcTextSize(TypeLabel);
	ImGui::PopFont();
	DrawList->AddText(TileFont, TileFontSize, ImVec2(TileMinimum.x + (TileWidth - LabelSize.x) * 0.5f, TileMinimum.y + 63.0f),
	                  ImGui::GetColorU32(ImGuiCol_Text), Label.c_str());
	DrawList->AddText(TileFont, TileFontSize, ImVec2(TileMinimum.x + (TileWidth - TypeSize.x) * 0.5f, TileMinimum.y + 90.0f),
	                  ImGui::GetColorU32(ImGuiCol_TextDisabled), TypeLabel);

	if (bHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		SelectedAssetPath = Asset.AssetPath;
	}
	if (bHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
		(Asset.Type == EAssetType::StaticMesh || Asset.Type == EAssetType::Material))
	{
		OpenAssetRequest = Asset.AssetId;
	}
	if (bHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		SelectedAssetPath = Asset.AssetPath;
		OpenContextMenu(Asset.FolderPath, Asset.AssetPath);
	}
	if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
	{
		ImGui::SetDragDropPayload("CONTENT_ASSET", Asset.AssetPath.c_str(), Asset.AssetPath.size() + 1);
		ImGui::TextUnformatted(Asset.Name.c_str());
		ImGui::EndDragDropSource();
	}
	ImGui::PopID();
}

// Content 항목과 빈 영역에서 공유하는 파일 작업 메뉴를 표시한다.
void FContentPanel::DrawContextMenu()
{
	if (ImGui::BeginPopup("ContentContext"))
	{
		const FAssetData* ContextAsset = ContextAssetPath.empty() ? nullptr : AssetRegistry.FindAsset(ContextAssetPath);
		const bool bHasActiveImports = AssetImportManager.HasActiveImports();
		if (ContextAsset && ContextAsset->HasSourceFile())
		{
			const bool bImporting = AssetImportManager.IsImporting(ContextAsset->SourceFilePath);
			if (ImGui::MenuItem(bImporting ? "Importing..." : "Import", nullptr, false, !bImporting))
			{
				ImportAsset();
			}
			ImGui::Separator();
		}
		if (ImGui::MenuItem("New Folder", nullptr, false, !bHasActiveImports))
		{
			CreateFolder(ContextFolderPath);
		}
		if (ImGui::MenuItem("Open In File Explorer"))
		{
			OpenInFileExplorer();
		}
		ImGui::Separator();
		const bool bCanEdit = !ContextAssetPath.empty() || ContextFolderPath != "/";
		ImGui::BeginDisabled(!bCanEdit);
		if (ImGui::MenuItem("Copy"))
		{
			CopyItem();
		}
		ImGui::EndDisabled();
		ImGui::BeginDisabled(CopiedItemType == EItemType::None || bHasActiveImports);
		if (ImGui::MenuItem("Paste"))
		{
			PasteItem(ContextFolderPath);
		}
		ImGui::EndDisabled();
		ImGui::BeginDisabled(!bCanEdit || bHasActiveImports);
		if (ImGui::MenuItem("Rename"))
		{
			const FAssetData* Asset = ContextAssetPath.empty() ? nullptr : AssetRegistry.FindAsset(ContextAssetPath);
			const FString ItemName = Asset ? Asset->Name : GetFolderName(ContextFolderPath);
			RenameText.fill('\0');
			std::copy_n(ItemName.begin(), std::min(ItemName.size(), RenameText.size() - 1), RenameText.begin());
			bOpenRenamePopup = true;
		}
		if (ImGui::MenuItem("Delete"))
		{
			bOpenDeleteConfirmation = true;
		}
		ImGui::EndDisabled();
		ImGui::EndPopup();
	}
	// Rename 요청을 이름 입력 Modal로 표시하고 유효한 변경만 적용한다.
	if (bOpenRenamePopup)
	{
		ImGui::OpenPopup("Rename Content");
		bOpenRenamePopup = false;
	}
	if (ImGui::BeginPopupModal("Rename Content", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		if (ImGui::IsWindowAppearing())
		{
			ImGui::SetKeyboardFocusHere();
		}
		ImGui::SetNextItemWidth(280.0f);
		const bool bSubmitted = ImGui::InputText("##RenameContent", RenameText.data(), RenameText.size(), ImGuiInputTextFlags_EnterReturnsTrue);
		const bool bRenameClicked = ImGui::Button("Rename", ImVec2(90.0f, 0.0f));
		if (bSubmitted || bRenameClicked)
		{
			if (RenameItem())
			{
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(90.0f, 0.0f)))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	// Delete 요청을 복구 불가능 경고 Modal로 표시한다.
	if (bOpenDeleteConfirmation)
	{
		ImGui::OpenPopup("Delete Content");
		bOpenDeleteConfirmation = false;
	}
	if (ImGui::BeginPopupModal("Delete Content", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		const FAssetData* Asset = ContextAssetPath.empty() ? nullptr : AssetRegistry.FindAsset(ContextAssetPath);
		const FString ItemName = Asset ? Asset->Name : GetFolderName(ContextFolderPath);
		ImGui::Text("Delete '%s'?", ItemName.c_str());
		ImGui::TextDisabled("This action cannot be undone.");
		ImGui::Spacing();
		if (ImGui::Button("Delete", ImVec2(90.0f, 0.0f)))
		{
			DeleteItem();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(90.0f, 0.0f)))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

// 선택 Folder를 변경하고 필요하면 탐색 History에 추가한다.
void FContentPanel::Navigate(const FString& FolderPath, bool bRecordHistory)
{
	if (SelectedFolderPath == FolderPath)
	{
		return;
	}
	SelectedFolderPath = FolderPath;
	SelectedAssetPath.clear();
	if (!bRecordHistory)
	{
		return;
	}
	NavigationHistory.erase(NavigationHistory.begin() + static_cast<int64>(NavigationIndex + 1), NavigationHistory.end());
	NavigationHistory.push_back(FolderPath);
	NavigationIndex = NavigationHistory.size() - 1;
}

// Registry를 다시 스캔하고 사라진 선택 경로를 정리한다.
void FContentPanel::Refresh()
{
	AssetRegistry.Scan();
	if (std::find(AssetRegistry.GetFolders().begin(), AssetRegistry.GetFolders().end(), SelectedFolderPath) == AssetRegistry.GetFolders().end())
	{
		Navigate("/");
	}
	if (!SelectedAssetPath.empty() && !AssetRegistry.FindAsset(SelectedAssetPath))
	{
		SelectedAssetPath.clear();
	}
}

// 다음 Frame에 열 Context Menu의 대상 Folder와 Asset을 보관한다.
void FContentPanel::OpenContextMenu(const FString& FolderPath, const FString& AssetPath)
{
	ContextFolderPath = FolderPath;
	ContextAssetPath = AssetPath;
	bOpenContextMenu = true;
}

// 대상 Folder 아래에 충돌하지 않는 New Folder를 만든다.
void FContentPanel::CreateFolder(const FString& ParentFolderPath)
{
	if (AssetImportManager.HasActiveImports())
	{
		return;
	}

	const std::filesystem::path ParentPath = FPaths::ResolveContentPath(ParentFolderPath);
	std::filesystem::path FolderPath = ParentPath / L"New Folder";
	for (uint32 Suffix = 2; std::filesystem::exists(FolderPath); ++Suffix)
	{
		FolderPath = ParentPath / (L"New Folder " + std::to_wstring(Suffix));
	}
	std::error_code FileSystemError;
	std::filesystem::create_directories(FolderPath, FileSystemError);
	if (FileSystemError)
	{
		KE_LOG(LogContentPanel, Error, "Folder 생성 실패. Error={}", FileSystemError.message());
		return;
	}
	bRefreshRequested = true;
}

// Context 대상 Asset이 속한 Folder 또는 Context Folder를 Windows File Explorer로 연다.
void FContentPanel::OpenInFileExplorer() const
{
	std::filesystem::path TargetPath;
	if (!ContextAssetPath.empty())
	{
		const FAssetData* Asset = AssetRegistry.FindAsset(ContextAssetPath);
		if (!Asset)
		{
			return;
		}
		TargetPath = FPaths::ResolveContentPath(Asset->FolderPath);
	}
	else
	{
		TargetPath = FPaths::ResolveContentPath(ContextFolderPath);
	}

	const FWString Parameters = L"\"" + TargetPath.wstring() + L"\"";
	const HINSTANCE Result = ShellExecuteW(nullptr, L"open", L"explorer.exe", Parameters.c_str(), nullptr, SW_SHOWNORMAL);
	if (reinterpret_cast<INT_PTR>(Result) <= 32)
	{
		KE_LOG(LogContentPanel, Warning, "File Explorer 실행 실패. Path={}", FPaths::ToUtf8(TargetPath.wstring()));
	}
}

// Context 대상 GLB와 출력 경로를 보관하고 다음 Frame에 Import 설정 Modal을 연다.
void FContentPanel::ImportAsset()
{
	const FAssetData* Asset = AssetRegistry.FindAsset(ContextAssetPath);
	if (!Asset || !Asset->HasSourceFile())
	{
		return;
	}

	PendingImportSourceFilePath = Asset->SourceFilePath;
	PendingImportDestinationAssetPath = Asset->FolderPath;
	PendingImportOptions = FGLBImportOptions();
	bOpenImportOptions = true;
}

// Context 대상의 논리 경로를 내부 Copy Clipboard에 저장한다.
void FContentPanel::CopyItem()
{
	if (!ContextAssetPath.empty())
	{
		CopiedItemType = EItemType::Asset;
		CopiedAssetPath = ContextAssetPath;
		CopiedFolderPath.clear();
	}
	else if (ContextFolderPath != "/")
	{
		CopiedItemType = EItemType::Folder;
		CopiedFolderPath = ContextFolderPath;
		CopiedAssetPath.clear();
	}
}

// Copy Clipboard의 Asset 또는 Folder를 대상 Folder에 복제한다.
void FContentPanel::PasteItem(const FString& FolderPath)
{
	if (AssetImportManager.HasActiveImports())
	{
		return;
	}

	const std::filesystem::path DestinationFolder = FPaths::ResolveContentPath(FolderPath);
	std::error_code FileSystemError;
	if (CopiedItemType == EItemType::Folder)
	{
		const std::filesystem::path Source = FPaths::ResolveContentPath(CopiedFolderPath);
		if (FPaths::IsInside(FolderPath, CopiedFolderPath))
		{
			return;
		}
		const std::filesystem::path Destination = MakeUniquePath(DestinationFolder / Source.filename(), true);
		std::filesystem::copy(Source, Destination, std::filesystem::copy_options::recursive, FileSystemError);
		if (!FileSystemError)
		{
			for (std::filesystem::recursive_directory_iterator It(Destination), End; It != End; ++It)
			{
				if (It->is_regular_file() && It->path().extension() == L".kasset" && !RegenerateAssetId(It->path()))
				{
					FileSystemError = std::make_error_code(std::errc::invalid_argument);
					break;
				}
			}
		}
	}
	else if (CopiedItemType == EItemType::Asset)
	{
		const FAssetData* Asset = AssetRegistry.FindAsset(CopiedAssetPath);
		if (!Asset)
		{
			return;
		}
		std::filesystem::path DestinationBase = DestinationFolder / FPaths::ToWide(Asset->Name);
		uint32 Suffix = 1;
		while (std::filesystem::exists(DestinationBase.wstring() + L".glb") || std::filesystem::exists(DestinationBase.wstring() + L".kasset"))
		{
			DestinationBase = DestinationFolder / (FPaths::ToWide(Asset->Name) + L" Copy" + (Suffix == 1 ? L"" : L" " + std::to_wstring(Suffix)));
			++Suffix;
		}
		if (Asset->HasSourceFile())
		{
			std::filesystem::copy_file(Asset->SourceFilePath, DestinationBase.wstring() + L".glb", FileSystemError);
		}
		if (!FileSystemError && Asset->HasBinaryFile())
		{
			const std::filesystem::path DestinationBinary = DestinationBase.wstring() + L".kasset";
			std::filesystem::copy_file(Asset->BinaryFilePath, DestinationBinary, FileSystemError);
			if (!FileSystemError && !RegenerateAssetId(DestinationBinary))
			{
				FileSystemError = std::make_error_code(std::errc::invalid_argument);
			}
		}
	}
	if (FileSystemError)
	{
		KE_LOG(LogContentPanel, Error, "Content 붙여넣기 실패. Error={}", FileSystemError.message());
		return;
	}
	bRefreshRequested = true;
}

// Context 대상 Asset의 파일명 또는 Folder 이름을 검증하고 실제 경로에 반영한다.
bool FContentPanel::RenameItem()
{
	if (AssetImportManager.HasActiveImports())
	{
		return false;
	}

	const FString NewName = RenameText.data();
	if (NewName.empty() || NewName == "." || NewName == ".." || NewName.find_first_of("<>:\"/\\|?*") != FString::npos ||
	    NewName.ends_with('.') || NewName.ends_with(' '))
	{
		KE_LOG(LogContentPanel, Warning, "사용할 수 없는 Content 이름. Name={}", NewName);
		return false;
	}

	std::error_code FileSystemError;
	if (!ContextAssetPath.empty())
	{
		const FAssetData* Asset = AssetRegistry.FindAsset(ContextAssetPath);
		if (!Asset || Asset->Name == NewName)
		{
			return Asset != nullptr;
		}

		const std::filesystem::path DestinationBase = FPaths::ResolveContentPath(Asset->FolderPath) / FPaths::ToWide(NewName);
		std::filesystem::path SourceDestination;
		std::filesystem::path BinaryDestination;
		if (Asset->HasSourceFile())
		{
			SourceDestination = DestinationBase;
			SourceDestination.replace_extension(Asset->SourceFilePath.extension());
		}
		if (Asset->HasBinaryFile())
		{
			BinaryDestination = DestinationBase;
			BinaryDestination.replace_extension(Asset->BinaryFilePath.extension());
		}
		if ((!SourceDestination.empty() && std::filesystem::exists(SourceDestination)) ||
		    (!BinaryDestination.empty() && std::filesystem::exists(BinaryDestination)))
		{
			KE_LOG(LogContentPanel, Warning, "같은 이름의 Content가 이미 존재한다. Name={}", NewName);
			return false;
		}

		bool bSourceRenamed = false;
		if (Asset->HasSourceFile())
		{
			std::filesystem::rename(Asset->SourceFilePath, SourceDestination, FileSystemError);
			bSourceRenamed = !FileSystemError;
		}
		if (!FileSystemError && Asset->HasBinaryFile())
		{
			std::filesystem::rename(Asset->BinaryFilePath, BinaryDestination, FileSystemError);
		}
		if (FileSystemError)
		{
			if (bSourceRenamed)
			{
				std::error_code RollbackError;
				std::filesystem::rename(SourceDestination, Asset->SourceFilePath, RollbackError);
			}
			KE_LOG(LogContentPanel, Error, "Content 이름 변경 실패. Error={}", FileSystemError.message());
			return false;
		}

		const FString NewAssetPath = FPaths::Combine(Asset->FolderPath, NewName);
		if (CopiedItemType == EItemType::Asset && CopiedAssetPath == ContextAssetPath)
		{
			CopiedAssetPath = NewAssetPath;
		}
		ContextAssetPath = NewAssetPath;
		SelectedAssetPath = NewAssetPath;
	}
	else
	{
		if (ContextFolderPath == "/" || GetFolderName(ContextFolderPath) == NewName)
		{
			return ContextFolderPath != "/";
		}
		const FString ParentPath = FPaths::GetPath(ContextFolderPath);
		const FString NewFolderPath = FPaths::Combine(ParentPath, NewName);
		const std::filesystem::path Source = FPaths::ResolveContentPath(ContextFolderPath);
		const std::filesystem::path Destination = FPaths::ResolveContentPath(NewFolderPath);
		if (std::filesystem::exists(Destination))
		{
			KE_LOG(LogContentPanel, Warning, "같은 이름의 Folder가 이미 존재한다. Name={}", NewName);
			return false;
		}
		std::filesystem::rename(Source, Destination, FileSystemError);
		if (FileSystemError)
		{
			KE_LOG(LogContentPanel, Error, "Folder 이름 변경 실패. Error={}", FileSystemError.message());
			return false;
		}

		const FString PreviousFolderPath = ContextFolderPath;
		const auto ReplaceFolderPrefix = [&PreviousFolderPath, &NewFolderPath](FString& FolderPath)
		{
			if (FPaths::IsInside(FolderPath, PreviousFolderPath))
			{
				FolderPath = NewFolderPath + FolderPath.substr(PreviousFolderPath.size());
			}
		};
		ReplaceFolderPrefix(SelectedFolderPath);
		ReplaceFolderPrefix(SelectedAssetPath);
		ReplaceFolderPrefix(ContextFolderPath);
		ReplaceFolderPrefix(CopiedFolderPath);
		ReplaceFolderPrefix(CopiedAssetPath);
		for (FString& HistoryPath : NavigationHistory)
		{
			ReplaceFolderPrefix(HistoryPath);
		}
	}
	bRefreshRequested = true;
	return true;
}

// Context 대상 Asset의 모든 파일 또는 Folder 전체를 확인 후 삭제한다.
void FContentPanel::DeleteItem()
{
	if (AssetImportManager.HasActiveImports())
	{
		return;
	}

	std::error_code FileSystemError;
	if (!ContextAssetPath.empty())
	{
		const FAssetData* Asset = AssetRegistry.FindAsset(ContextAssetPath);
		if (!Asset)
		{
			return;
		}
		if (Asset->HasSourceFile())
		{
			std::filesystem::remove(Asset->SourceFilePath, FileSystemError);
		}
		if (!FileSystemError && Asset->HasBinaryFile())
		{
			std::filesystem::remove(Asset->BinaryFilePath, FileSystemError);
		}
		if (CopiedItemType == EItemType::Asset && CopiedAssetPath == ContextAssetPath)
		{
			CopiedItemType = EItemType::None;
			CopiedAssetPath.clear();
		}
		SelectedAssetPath.clear();
	}
	else if (ContextFolderPath != "/")
	{
		std::filesystem::remove_all(FPaths::ResolveContentPath(ContextFolderPath), FileSystemError);
		if (CopiedItemType == EItemType::Folder && FPaths::IsInside(CopiedFolderPath, ContextFolderPath))
		{
			CopiedItemType = EItemType::None;
			CopiedFolderPath.clear();
		}
		if (CopiedItemType == EItemType::Asset && FPaths::IsInside(CopiedAssetPath, ContextFolderPath))
		{
			CopiedItemType = EItemType::None;
			CopiedAssetPath.clear();
		}
	}
	if (FileSystemError)
	{
		KE_LOG(LogContentPanel, Error, "Content 삭제 실패. Error={}", FileSystemError.message());
		return;
	}
	bRefreshRequested = true;
}

// Content 항목 종류에 따라 논리 Asset 또는 Folder를 대상 Folder 아래로 이동한다.
void FContentPanel::MoveItem(EItemType ItemType, const FString& SourcePath, const FString& DestinationFolderPath)
{
	if (AssetImportManager.HasActiveImports())
	{
		return;
	}

	if (ItemType == EItemType::Asset)
	{
		const FAssetData* Asset = AssetRegistry.FindAsset(SourcePath);
		if (!Asset || Asset->FolderPath == DestinationFolderPath)
		{
			return;
		}

		const std::filesystem::path DestinationFolder = FPaths::ResolveContentPath(DestinationFolderPath);
		std::filesystem::path DestinationBase = DestinationFolder / FPaths::ToWide(Asset->Name);
		uint32 Suffix = 1;
		while (std::filesystem::exists(DestinationBase.wstring() + L".glb") || std::filesystem::exists(DestinationBase.wstring() + L".kasset"))
		{
			DestinationBase = DestinationFolder / (FPaths::ToWide(Asset->Name) + L" " + std::to_wstring(Suffix++));
		}

		std::error_code FileSystemError;
		if (Asset->HasSourceFile())
		{
			std::filesystem::rename(Asset->SourceFilePath, DestinationBase.wstring() + L".glb", FileSystemError);
		}
		if (!FileSystemError && Asset->HasBinaryFile())
		{
			std::filesystem::rename(Asset->BinaryFilePath, DestinationBase.wstring() + L".kasset", FileSystemError);
		}
		if (FileSystemError)
		{
			KE_LOG(LogContentPanel, Error, "Content 이동 실패. Error={}", FileSystemError.message());
			return;
		}
		SelectedAssetPath.clear();
		bRefreshRequested = true;
		return;
	}
	if (ItemType != EItemType::Folder)
	{
		return;
	}

	const FString& SourceFolderPath = SourcePath;
	if (SourceFolderPath == "/" || FPaths::IsInside(DestinationFolderPath, SourceFolderPath))
	{
		return;
	}
	const FString SourceParentPath = FPaths::GetPath(SourceFolderPath);
	if (SourceParentPath == DestinationFolderPath)
	{
		return;
	}

	const std::filesystem::path Source = FPaths::ResolveContentPath(SourceFolderPath);
	const std::filesystem::path Destination = MakeUniquePath(FPaths::ResolveContentPath(DestinationFolderPath) / Source.filename(), true);
	std::error_code FileSystemError;
	std::filesystem::rename(Source, Destination, FileSystemError);
	if (FileSystemError)
	{
		KE_LOG(LogContentPanel, Error, "Folder 이동 실패. Error={}", FileSystemError.message());
		return;
	}

	const FString DestinationName = FPaths::ToUtf8(Destination.filename().wstring());
	const FString NewFolderPath = FPaths::Combine(DestinationFolderPath, DestinationName);
	const auto ReplaceFolderPrefix = [&SourceFolderPath, &NewFolderPath](FString& Path)
	{
		if (FPaths::IsInside(Path, SourceFolderPath))
		{
			Path = NewFolderPath + Path.substr(SourceFolderPath.size());
		}
	};
	ReplaceFolderPrefix(SelectedFolderPath);
	ReplaceFolderPrefix(SelectedAssetPath);
	ReplaceFolderPrefix(ContextFolderPath);
	ReplaceFolderPrefix(ContextAssetPath);
	ReplaceFolderPrefix(CopiedFolderPath);
	ReplaceFolderPrefix(CopiedAssetPath);
	for (FString& HistoryPath : NavigationHistory)
	{
		ReplaceFolderPrefix(HistoryPath);
	}
	bRefreshRequested = true;
}

// Folder 항목을 Content Drag의 Drop Target으로 만들고 Preview 중에 파란 Hover 테두리를 그린다.
void FContentPanel::DropItem(const FString& FolderPath, const ImVec2& Minimum, const ImVec2& Maximum)
{
	if (!ImGui::BeginDragDropTarget())
	{
		return;
	}
	const ImGuiPayload* AssetPayload = ImGui::AcceptDragDropPayload("CONTENT_ASSET", ImGuiDragDropFlags_AcceptBeforeDelivery);
	const ImGuiPayload* FolderPayload = ImGui::AcceptDragDropPayload("CONTENT_FOLDER", ImGuiDragDropFlags_AcceptBeforeDelivery);
	if (AssetPayload || FolderPayload)
	{
		ImGui::GetForegroundDrawList()->AddRect(Minimum, Maximum, DropHighlightColor, 3.0f, 0, 2.0f);
		if (AssetPayload && AssetPayload->IsDelivery())
		{
			MoveItem(EItemType::Asset, static_cast<const char*>(AssetPayload->Data), FolderPath);
		}
		else if (FolderPayload && FolderPayload->IsDelivery())
		{
			MoveItem(EItemType::Folder, static_cast<const char*>(FolderPayload->Data), FolderPath);
		}
	}
	ImGui::EndDragDropTarget();
}

// 검색에 일치하는 자신 또는 자손 Folder가 있는지 확인한다.
bool FContentPanel::IsFolderVisible(const FString& FolderPath) const
{
	if (!FolderSearchText[0] || FolderPath == "/" || IsFolderSearchMatch(FolderPath))
	{
		return true;
	}
	for (const FString& Candidate : AssetRegistry.GetFolders())
	{
		if (Candidate != FolderPath && FPaths::IsInside(Candidate, FolderPath) && IsFolderSearchMatch(Candidate))
		{
			return true;
		}
	}
	return false;
}

// Folder 이름 또는 경로가 왼쪽 검색어와 일치하는지 확인한다.
bool FContentPanel::IsFolderSearchMatch(const FString& FolderPath) const
{
	return ContainsText(FolderPath, FolderSearchText.data());
}

// Asset 이름 또는 논리 경로가 오른쪽 검색어와 일치하는지 확인한다.
bool FContentPanel::IsAssetSearchMatch(const FAssetData& Asset) const
{
	return ContainsText(Asset.Name, AssetSearchText.data()) || ContainsText(Asset.AssetPath, AssetSearchText.data());
}

// Folder가 Parent 바로 아래에 위치하는지 확인한다.
bool FContentPanel::IsDirectChildFolder(const FString& FolderPath, const FString& ParentPath) const
{
	if (FolderPath == "/" || FolderPath == ParentPath)
	{
		return false;
	}
	return FPaths::GetPath(FolderPath) == ParentPath;
}

// 논리 Folder 경로의 마지막 이름을 반환한다.
FString FContentPanel::GetFolderName(const FString& FolderPath) const
{
	if (FolderPath == "/")
	{
		return "Content";
	}
	return FolderPath.substr(FolderPath.find_last_of('/') + 1);
}

// 원본 이름을 유지하면서 충돌 시 Copy 번호를 붙인 경로를 반환한다.
std::filesystem::path FContentPanel::MakeUniquePath(const std::filesystem::path& DesiredPath, bool bDirectory) const
{
	if (!std::filesystem::exists(DesiredPath))
	{
		return DesiredPath;
	}
	for (uint32 Suffix = 1;; ++Suffix)
	{
		const FWString SuffixText = L" Copy" + (Suffix == 1 ? FWString() : L" " + std::to_wstring(Suffix));
		const std::filesystem::path Candidate = bDirectory
		                                            ? DesiredPath.parent_path() / (DesiredPath.filename().wstring() + SuffixText)
		                                            : DesiredPath.parent_path() /
		                                                  (DesiredPath.stem().wstring() + SuffixText + DesiredPath.extension().wstring());
		if (!std::filesystem::exists(Candidate))
		{
			return Candidate;
		}
	}
}
