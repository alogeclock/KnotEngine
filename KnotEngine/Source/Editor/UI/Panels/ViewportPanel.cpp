#include "UI/Panels/ViewportPanel.h"

#include "Input/InputRouter.h"
#include "Render/ImGui/ImGuiRenderBackend.h"
#include "Runtime/EditorEngine.h"
#include "Viewport/LevelEditorViewportClient.h"
#include "Viewport/Viewport.h"

#include <imgui.h>
#include <cmath>

FViewportPanel::FViewportPanel(
	UEditorEngine& InEditorEngine,
	IRenderDevice& InRenderDevice,
	IImGuiRenderBackend& InRenderBackend,
	FInputRouter& InInputRouter)
	: EditorEngine(InEditorEngine), Viewport(InRenderDevice), ViewportClient(InEditorEngine, Viewport),
	  RenderBackend(InRenderBackend), InputRouter(InInputRouter)
{
	EditorEngine.RegisterViewportClient(ViewportClient);
}

FViewportPanel::~FViewportPanel()
{
	EditorEngine.UnregisterViewportClient(ViewportClient);
}

void FViewportPanel::Release()
{
	Viewport.Release();
}

void FViewportPanel::Draw(bool bVisible)
{
	if (!bVisible)
	{
		Viewport.Release();
		return;
	}

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	// 접힌 Panel은 이미지를 표시하지 않으므로 offscreen target을 해제하여 World 렌더링을 중단한다.
	if (!ImGui::Begin("Viewport"))
	{
		Viewport.Release();
		ImGui::End();
		ImGui::PopStyleVar();
		return;
	}

	// Docking 또는 창 축소로 content 영역이 사라진 동안에는 유효한 Render Target을 만들 수 없다.
	const ImVec2 ImageSize = ImGui::GetContentRegionAvail();
	if (ImageSize.x <= 0.0f || ImageSize.y <= 0.0f)
	{
		Viewport.Release();
		ImGui::End();
		ImGui::PopStyleVar();
		return;
	}

	// ImGui 표시 좌표를 framebuffer pixel 크기로 변환하여 DPI 배율에서도 offscreen 이미지가 늘어나거나 흐려지지 않게 한다.
	const ImVec2 FramebufferScale = ImGui::GetIO().DisplayFramebufferScale;
	const uint32 Width = static_cast<uint32>(std::lround(ImageSize.x * FramebufferScale.x));
	const uint32 Height = static_cast<uint32>(std::lround(ImageSize.y * FramebufferScale.y));
	if (Width == 0 || Height == 0)
	{
		Viewport.Release();
		ImGui::End();
		ImGui::PopStyleVar();
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

	ImGui::End();
	ImGui::PopStyleVar();
}
