#include "UI/Panels/ViewportPanel.h"

#include "Input/InputRouter.h"
#include "Render/ImGui/ImGuiRenderBackend.h"
#include "Viewport/LevelEditorViewportClient.h"
#include "Viewport/Viewport.h"

#include <imgui.h>
#include <algorithm>

FViewportPanel::FViewportPanel(
	FViewport& InViewport,
	FLevelEditorViewportClient& InViewportClient,
	IImGuiRenderBackend& InRenderBackend,
	FInputRouter& InInputRouter)
	: Viewport(InViewport), ViewportClient(InViewportClient), RenderBackend(InRenderBackend), InputRouter(InInputRouter)
{
}

void FViewportPanel::Draw()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	if (!ImGui::Begin("Viewport"))
	{
		ImGui::End();
		ImGui::PopStyleVar();
		return;
	}
	const ImVec2 Available = ImGui::GetContentRegionAvail();
	const ImVec2 ImageSize(std::max(Available.x, 1.0f), std::max(Available.y, 1.0f));
	const uint32 Width = static_cast<uint32>(ImageSize.x);
	const uint32 Height = static_cast<uint32>(ImageSize.y);
	Viewport.Resize(Width, Height);
	if (Viewport.IsValid())
	{
		const ImTextureID TextureId = RenderBackend.GetImGuiTextureID(Viewport.GetColorTarget());
		ImGui::Image(ImTextureRef(TextureId), ImageSize);
		// 매 프레임 InputRouter에 ViewportClient를 등록하여 ImGui의 Hovered/Focused 상태를 전달한다.
		InputRouter.RegisterTarget(ViewportClient, ImGui::IsItemHovered(), ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows));
	}
	ImGui::End();
	ImGui::PopStyleVar();
}
