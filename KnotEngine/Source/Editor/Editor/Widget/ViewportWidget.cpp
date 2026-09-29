#include "Editor/Widget/ViewportWidget.h"

#include "Input/InputRouter.h"
#include "Viewport/EditorViewportClient.h"

#include <cmath>
#include <imgui.h>

FViewportWidget::FViewportWidget(FRenderSystem& InRenderSystem, FInputRouter& InInputRouter)
	: InputRouter(InInputRouter), Viewport(InRenderSystem)
{
}

// offscreen 결과를 표시하고 Active Pane의 이미지 영역만 Focused 입력 대상으로 등록한다.
bool FViewportWidget::Draw(FEditorViewportClient& ViewportClient, bool bFocused)
{
	const ImVec2 ImageSize = ImGui::GetContentRegionAvail();
	if (ImageSize.x <= 0.0f || ImageSize.y <= 0.0f)
	{
		Release(ViewportClient);
		return false;
	}

	const ImVec2 FramebufferScale = ImGui::GetIO().DisplayFramebufferScale;
	const uint32 Width = static_cast<uint32>(std::lround(ImageSize.x * FramebufferScale.x));
	const uint32 Height = static_cast<uint32>(std::lround(ImageSize.y * FramebufferScale.y));
	if (Width == 0 || Height == 0)
	{
		Release(ViewportClient);
		return false;
	}

	Viewport.Resize(Width, Height);
	if (!Viewport.IsValid())
	{
		InputRouter.UnregisterTarget(ViewportClient);
		return false;
	}

	ImGui::Image(ImTextureRef(Viewport.GetDisplayTextureId()), ImageSize);
	const ImVec2 ImagePosition = ImGui::GetItemRectMin();
	ViewportClient.SetInputRect(FVector2(ImagePosition.x, ImagePosition.y), FVector2(ImageSize.x, ImageSize.y));
	const bool bImageHovered = ImGui::IsItemHovered();
	const bool bViewportFocused = bFocused && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
	InputRouter.RegisterTarget(ViewportClient, bImageHovered, bViewportFocused);
	return true;
}

void FViewportWidget::Release(FEditorViewportClient& ViewportClient)
{
	InputRouter.UnregisterTarget(ViewportClient);
	Viewport.Release();
}
