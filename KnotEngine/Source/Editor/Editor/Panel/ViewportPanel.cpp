#include "Editor/Panel/ViewportPanel.h"

#include "Editor/Toolbar/ViewportToolbar.h"

#include <imgui.h>

FViewportPanel::FViewportPanel(
	FRenderSystem& InRenderSystem,
	FInputRouter& InInputRouter,
	const FViewportStatState& InStatState,
	FEditorSelection& InSelection)
	: ViewportWidget(InRenderSystem, InInputRouter), ViewportClient(ViewportWidget.GetViewport(), InSelection), ViewportOverlayWidget(InStatState)
{
}

FViewportPanel::~FViewportPanel()
{
	ViewportWidget.Release(ViewportClient);
}

void FViewportPanel::Release()
{
	ViewportWidget.Release(ViewportClient);
}

void FViewportPanel::Draw(bool bVisible, float DeltaTime, FViewportToolbar& Toolbar)
{
	ViewportOverlayWidget.Tick(DeltaTime);
	if (!bVisible)
	{
		ViewportWidget.Release(ViewportClient);
		return;
	}

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	// 접힌 Panel은 이미지를 표시하지 않으므로 offscreen target을 해제하여 World 렌더링을 중단한다.
	if (!ImGui::Begin("Viewport"))
	{
		ViewportWidget.Release(ViewportClient);
		ImGui::End();
		ImGui::PopStyleVar();
		return;
	}

	Toolbar.Draw(ViewportClient);
	if (ViewportWidget.Draw(ViewportClient))
	{
		ViewportOverlayWidget.Draw();
	}

	ImGui::End();
	ImGui::PopStyleVar();
}
