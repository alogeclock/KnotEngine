#include "ImGui/Panels/ViewportPanel.h"

#include "Input/InputRouter.h"
#include "Render/ImGui/ImGuiRenderBackend.h"
#include "Viewport/Level/LevelEditorViewportClient.h"
#include "Viewport/Viewport.h"

#include <imgui.h>
#include <cmath>
#include <iterator>

FViewportPanel::FViewportPanel(
	IRenderDevice& InRenderDevice,
	IImGuiRenderBackend& InRenderBackend,
	FInputRouter& InInputRouter)
	: Viewport(InRenderDevice), ViewportClient(Viewport), RenderBackend(InRenderBackend), InputRouter(InInputRouter)
{
}

FViewportPanel::~FViewportPanel()
{
	InputRouter.UnregisterTarget(ViewportClient);
}

void FViewportPanel::Release()
{
	Viewport.Release();
}

void FViewportPanel::Draw(bool bVisible)
{
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
				float ViewRotation[3] = { ViewTransform.ViewRotation.Pitch, ViewTransform.ViewRotation.Yaw, ViewTransform.ViewRotation.Roll, };
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
					&Camera.CameraSpeed,
					FEditorViewportClient::MinCameraSpeed,
					FEditorViewportClient::MaxCameraSpeed,
					"%.1f");
				ImGui::SameLine();
				if (ImGui::Button("Reset"))
				{
					Camera.CameraSpeed = FEditorViewportClient::DefaultCameraSpeed;
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
	}
	ImGui::EndChild();
	ImGui::PopStyleVar();
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
		const ImTextureID TextureId = RenderBackend.GetImGuiTextureID(Viewport.GetColorTarget());
		ImGui::Image(ImTextureRef(TextureId), ImageSize);
		// 매 프레임 InputRouter에 ViewportClient를 등록하여 ImGui의 Hovered/Focused 상태를 전달한다.
		InputRouter.RegisterTarget(ViewportClient, ImGui::IsItemHovered(), ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows));
	}
}
