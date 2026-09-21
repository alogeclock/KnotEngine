#include "Editor/Panels/ViewportPanel.h"

#include "Input/InputRouter.h"
#include "Render/RenderSystem.h"

#include <cmath>
#include <imgui.h>

FViewportPanel::FViewportPanel(
	FRenderSystem& InRenderSystem,
	FInputRouter& InInputRouter,
	const FViewportStatState& InStatState,
	FEditorSelection& InSelection)
	: Viewport(InRenderSystem), ViewportClient(Viewport, InSelection), StatOverlay(InStatState), Toolbar(InRenderSystem), InputRouter(InInputRouter)
{
}

FViewportPanel::~FViewportPanel()
{
	InputRouter.UnregisterTarget(ViewportClient);
}

void FViewportPanel::Release()
{
	Viewport.Release();
	Toolbar.Release();
}

void FViewportPanel::Startup()
{
	Toolbar.Startup();
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

	Toolbar.Draw(ViewportClient);
	DrawViewport();

	ImGui::End();
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
	if (!Viewport.IsValid())
	{
		return;
	}

	ImGui::Image(ImTextureRef(Viewport.GetDisplayTextureId()), ImageSize);
	const ImVec2 ImagePosition = ImGui::GetItemRectMin();
	ViewportClient.SetInputRect(FVector2(ImagePosition.x, ImagePosition.y), FVector2(ImageSize.x, ImageSize.y));
	const bool bImageHovered = ImGui::IsItemHovered();
	const bool bViewportFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
	StatOverlay.Draw();
	// 매 프레임 InputRouter에 ViewportClient를 등록하여 ImGui의 Hovered/Focused 상태를 전달한다.
	InputRouter.RegisterTarget(ViewportClient, bImageHovered, bViewportFocused);
}
