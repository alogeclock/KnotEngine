#include "Editor/Panels/ViewportPanel.h"

#include "Core/IO/Paths.h"
#include "Input/InputRouter.h"
#include "Render/RenderSystem.h"
#include "Viewport/Level/LevelEditorViewportClient.h"
#include "Viewport/Viewport.h"

#include <cmath>
#include <imgui.h>
#include <iterator>
#include <algorithm>
#include <Windows.h>
#include <objbase.h>
#include <wincodec.h>
#include <wrl/client.h>

FViewportPanel::FViewportPanel(
	FRenderSystem& InRenderSystem,
	FInputRouter& InInputRouter,
	const FViewportStatState& InStatState,
	FEditorSelection& InSelection)
	: Viewport(InRenderSystem), ViewportClient(Viewport, InSelection), StatOverlay(InStatState), RenderSystem(InRenderSystem), InputRouter(InInputRouter)
{
}

FViewportPanel::~FViewportPanel()
{
	InputRouter.UnregisterTarget(ViewportClient);
}

void FViewportPanel::Release()
{
	Viewport.Release();
	RenderSystem.DestroyTexture(ViewModeIcons);
	ViewModeIconsId = {};
}

// PNG Atlas를 시작 시 한 번 디코딩하고 ImGui Texture ID를 캐시한다.
void FViewportPanel::Startup()
{
	constexpr uint32 AtlasWidth = 768;
	constexpr uint32 AtlasHeight = 256;
	const HRESULT InitializeResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	TArray<uint8> Pixels(AtlasWidth * AtlasHeight * 4);
	bool bDecoded = false;
	{
		Microsoft::WRL::ComPtr<IWICImagingFactory> Factory;
		Microsoft::WRL::ComPtr<IWICBitmapDecoder> Decoder;
		Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> Frame;
		Microsoft::WRL::ComPtr<IWICBitmapScaler> Scaler;
		Microsoft::WRL::ComPtr<IWICFormatConverter> Converter;
		const std::filesystem::path FilePath = std::filesystem::path(FPaths::ContentDir()) / L"Engine/Icon/ViewportViewModes.png";
		if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&Factory))) &&
			SUCCEEDED(Factory->CreateDecoderFromFilename(FilePath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &Decoder)) &&
			SUCCEEDED(Decoder->GetFrame(0, &Frame)) && SUCCEEDED(Factory->CreateBitmapScaler(&Scaler)) &&
			SUCCEEDED(Scaler->Initialize(Frame.Get(), AtlasWidth, AtlasHeight, WICBitmapInterpolationModeFant)) &&
			SUCCEEDED(Factory->CreateFormatConverter(&Converter)) &&
			SUCCEEDED(Converter->Initialize(Scaler.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)))
		{
			bDecoded = SUCCEEDED(Converter->CopyPixels(nullptr, AtlasWidth * 4, static_cast<UINT>(Pixels.size()), Pixels.data()));
		}
	}
	if (SUCCEEDED(InitializeResult))
	{
		CoUninitialize();
	}
	panicf(bDecoded, "Viewport View Mode Icon Atlas를 디코딩하지 못했다.");
	FTextureDesc Desc;
	Desc.Width = AtlasWidth;
	Desc.Height = AtlasHeight;
	Desc.Format = ETextureFormat::RGBA8UNorm;
	Desc.Usage = ETextureUsage::ShaderResource;
	const FTextureSubresourceData Data = { Pixels, AtlasWidth * 4, static_cast<uint32>(Pixels.size()) };
	ViewModeIcons = RenderSystem.CreateTexture(Desc, std::span<const FTextureSubresourceData>(&Data, 1));
	ViewModeIconsId = RenderSystem.GetImGuiTextureID(ViewModeIcons);
}

void FViewportPanel::Draw(bool bVisible, float DeltaTime)
{
	StatOverlay.Tick(DeltaTime);
	if (!bVisible)
	{
		InputRouter.UnregisterTarget(ViewportClient);
		Viewport.Release();
		return;
	}

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	// 접힌 Panel은 이미지를 표시하지 않으므로 offscreen target을 해제하여 World 렌더링을 중단한다.
	if (!ImGui::Begin("Viewport"))
	{
		InputRouter.UnregisterTarget(ViewportClient);
		Viewport.Release();
		ImGui::End();
		ImGui::PopStyleVar();
		return;
	}

	DrawToolbar();
	DrawViewport();

	ImGui::End();
	ImGui::PopStyleVar();
}

void FViewportPanel::DrawToolbar()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));
	const float ToolbarHeight = ImGui::GetFrameHeight() + 8.0f;
	if (ImGui::BeginChild("##ViewportToolbar", ImVec2(0.0f, ToolbarHeight), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar))
	{
		if (ImGui::Button("Camera"))
		{
			ImGui::OpenPopup("##CameraMenu");
		}
		const ImVec2 CameraPopupPosition(ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y);
		ImGui::SetNextWindowPos(CameraPopupPosition, ImGuiCond_Appearing);
		if (ImGui::BeginPopup("##CameraMenu"))
		{
			FEditorViewportCamera& Camera = ViewportClient.GetCamera();
			FEditorViewportCameraTransform& ViewTransform = Camera.ViewTransform;
			bool bViewTransformChanged = false;
			ImGui::Dummy(ImVec2(0.0f, 1.0f));
			ImGui::TextDisabled("Camera");
			ImGui::Separator();

			if (ImGui::BeginTable("##CameraSettings", 2, ImGuiTableFlags_SizingStretchProp, ImVec2(380.0f, 0.0f)))
			{
				ImGui::TableSetupColumn("##CameraSettingName", ImGuiTableColumnFlags_WidthFixed, 80.0f);
				ImGui::TableSetupColumn("##CameraSettingValue", ImGuiTableColumnFlags_WidthStretch);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted("Location");
				ImGui::TableSetColumnIndex(1);
				float ViewLocation[3] = { ViewTransform.ViewLocation.X, ViewTransform.ViewLocation.Y, ViewTransform.ViewLocation.Z };
				ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::DragFloat3("##ViewLocation", ViewLocation, 0.1f, 0.0f, 0.0f, "%.2f"))
				{
					ViewTransform.ViewLocation = FVector(ViewLocation[0], ViewLocation[1], ViewLocation[2]);
					bViewTransformChanged = true;
				}

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted("Rotation");
				ImGui::TableSetColumnIndex(1);

				float ViewRotation[3] = { ViewTransform.ViewRotation.Pitch, ViewTransform.ViewRotation.Yaw, ViewTransform.ViewRotation.Roll };
				ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::DragFloat3("##ViewRotation", ViewRotation, 0.1f, 0.0f, 0.0f, "%.2f"))
				{
					ViewTransform.ViewRotation = FRotator(ViewRotation[0], ViewRotation[1], ViewRotation[2]);
					bViewTransformChanged = true;
				}

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted("FOV");
				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-1.0f);
				ImGui::BeginDisabled(ViewTransform.bIsOrtho);
				ImGui::DragFloat("##CameraFOV", &ViewTransform.FOV, 0.1f, 30.0f, 120.0f, "%.1f deg", ImGuiSliderFlags_AlwaysClamp);
				ImGui::EndDisabled();

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted("Sensitivity");
				ImGui::TableSetColumnIndex(1);
				const ImGuiStyle& Style = ImGui::GetStyle();
				const float ResetButtonWidth = ImGui::CalcTextSize("Reset").x + Style.FramePadding.x * 2.0f;
				ImGui::SetNextItemWidth(-ResetButtonWidth - Style.ItemSpacing.x);
				ImGui::SliderFloat(
				    "##CameraSensitivity",
				    &Camera.Sensitivity,
				    FEditorViewportClient::MinCameraSensitivity,
				    FEditorViewportClient::MaxCameraSensitivity,
				    "%.1f");
				ImGui::SameLine();
				if (ImGui::Button("Reset"))
				{
					Camera.Sensitivity = FEditorViewportClient::DefaultCameraSensitivity;
				}

				ImGui::EndTable();
			}

			if (bViewTransformChanged)
			{
				ViewportClient.OnViewTransformChanged();
			}

			ImGui::Dummy(ImVec2(0.0f, 2.0f));
			ImGui::EndPopup();
		}

		ImGui::SameLine();
		if (ImGui::Button("View"))
		{
			ImGui::OpenPopup("##ViewMenu");
		}
		const ImVec2 ViewPopupPosition(ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y);
		ImGui::SetNextWindowPos(ViewPopupPosition, ImGuiCond_Appearing);
		if (ImGui::BeginPopup("##ViewMenu"))
		{
			FEditorViewportCamera& Camera = ViewportClient.GetCamera();
			ImGui::Dummy(ImVec2(0.0f, 1.0f));
			ImGui::TextDisabled("Perspective");
			ImGui::Separator();
			if (ImGui::RadioButton("Perspective", Camera.ViewMode == EEditorViewportViewMode::Perspective))
			{
				Camera.ViewMode = EEditorViewportViewMode::Perspective;
				ViewportClient.OnCameraStateChanged();
				ImGui::CloseCurrentPopup();
			}

			ImGui::Spacing();
			ImGui::TextDisabled("Orthographic");
			ImGui::Separator();
			static constexpr EEditorViewportViewMode OrthographicViewModes[] = {
				EEditorViewportViewMode::Top,
				EEditorViewportViewMode::Bottom,
				EEditorViewportViewMode::Left,
				EEditorViewportViewMode::Right,
				EEditorViewportViewMode::Front,
				EEditorViewportViewMode::Back,
			};
			static constexpr const char* OrthographicViewNames[] = { "Top", "Bottom", "Left", "Right", "Front", "Back" };
			static_assert(std::size(OrthographicViewModes) == std::size(OrthographicViewNames));
			for (SIZE_T Index = 0; Index < std::size(OrthographicViewModes); ++Index)
			{
				if (ImGui::RadioButton(OrthographicViewNames[Index], Camera.ViewMode == OrthographicViewModes[Index]))
				{
					Camera.ViewMode = OrthographicViewModes[Index];
					ViewportClient.OnCameraStateChanged();
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::Dummy(ImVec2(0.0f, 2.0f));
			ImGui::EndPopup();
		}

		ImGui::SameLine();
		if (ImGui::Button("Show"))
		{
			ImGui::OpenPopup("##ShowMenu");
		}
		const ImVec2 ShowPopupPosition(ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y);
		ImGui::SetNextWindowPos(ShowPopupPosition, ImGuiCond_Appearing);
		if (ImGui::BeginPopup("##ShowMenu"))
		{
			FShowFlags& ShowFlags = ViewportClient.GetShowFlags();
			ImGui::MenuItem("Primitives", nullptr, &ShowFlags.bPrimitive);
			ImGui::MenuItem("Grid", nullptr, &ShowFlags.bGrid);
			ImGui::MenuItem("Axis", nullptr, &ShowFlags.bAxis);
			ImGui::MenuItem("Bounds", nullptr, &ShowFlags.bBounds);
			ImGui::EndPopup();
		}
		DrawViewModeButtons();
	}
	ImGui::EndChild();
	ImGui::PopStyleVar();
}

void FViewportPanel::DrawViewModeButtons()
{
	static constexpr const char* Names[] = { "Wireframe", "Shaded Wireframe", "Unlit", "Lit" };
	static constexpr EViewMode Modes[] = { EViewMode::Wireframe, EViewMode::ShadedWireframe, EViewMode::Unlit };
	static constexpr float IconCenters[] = { 0.155f, 0.385f, 0.615f, 0.845f };
	const float IconSize = ImGui::GetFontSize() + 2.0f;
	const float ButtonWidth = IconSize + 8.0f;
	const float TotalWidth = ButtonWidth * 4.0f + 2.0f * 3.0f;
	ImGui::SameLine();
	ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x - TotalWidth));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, ImGui::GetStyle().FramePadding.y - 1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f, 0.0f));
	for (SIZE_T Index = 0; Index < 4; ++Index)
	{
		if (Index != 0)
		{
			ImGui::SameLine();
		}
		const bool bLit = Index == 3;
		const bool bSelected = !bLit && ViewportClient.GetViewMode() == Modes[Index];
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(bSelected ? ImGuiCol_ButtonActive : ImGuiCol_FrameBg));
		ImGui::BeginDisabled(bLit);
		const ImVec2 UV0(IconCenters[Index] - 0.09f, 0.225f);
		const ImVec2 UV1(IconCenters[Index] + 0.09f, 0.765f);
		if (ImGui::ImageButton(Names[Index], ImTextureRef(ViewModeIconsId), ImVec2(IconSize, IconSize), UV0, UV1) && !bLit)
		{
			ViewportClient.GetViewMode() = Modes[Index];
		}
		ImGui::EndDisabled();
		ImGui::PopStyleColor();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			ImGui::SetTooltip("%s", Names[Index]);
		}
	}
	ImGui::PopStyleVar(2);
}

void FViewportPanel::DrawViewport()
{
	// Docking 또는 창 축소로 content 영역이 사라진 동안에는 유효한 Render Target을 만들 수 없다.
	const ImVec2 ImageSize = ImGui::GetContentRegionAvail();
	if (ImageSize.x <= 0.0f || ImageSize.y <= 0.0f)
	{
		InputRouter.UnregisterTarget(ViewportClient);
		Viewport.Release();
		return;
	}

	// ImGui 표시 좌표를 framebuffer pixel 크기로 변환하여 DPI 배율에서도 offscreen 이미지가 늘어나거나 흐려지지 않게 한다.
	const ImVec2 FramebufferScale = ImGui::GetIO().DisplayFramebufferScale;
	const uint32 Width = static_cast<uint32>(std::lround(ImageSize.x * FramebufferScale.x));
	const uint32 Height = static_cast<uint32>(std::lround(ImageSize.y * FramebufferScale.y));
	if (Width == 0 || Height == 0)
	{
		InputRouter.UnregisterTarget(ViewportClient);
		Viewport.Release();
		return;
	}

	Viewport.Resize(Width, Height);
	if (Viewport.IsValid())
	{
		ImGui::Image(ImTextureRef(Viewport.GetDisplayTextureId()), ImageSize);
		const ImVec2 ImagePosition = ImGui::GetItemRectMin();
		ViewportClient.SetInputRect(FVector2(ImagePosition.x, ImagePosition.y), FVector2(ImageSize.x, ImageSize.y));
		const bool bImageHovered = ImGui::IsItemHovered();
		const bool bViewportFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		StatOverlay.Draw();
		// 매 프레임 InputRouter에 ViewportClient를 등록하여 ImGui의 Hovered/Focused 상태를 전달한다.
		InputRouter.RegisterTarget(ViewportClient, bImageHovered, bViewportFocused);
	}
}
