#include "Editor/Panel/ViewportPanel.h"

#include "Component/TransformComponent.h"
#include "Editor/EditorSelection.h"
#include "Editor/Toolbar/ViewportToolbar.h"
#include "Editor/Widget/NodeCreationMenu.h"
#include "World/Node.h"
#include "World/World.h"

#include <imgui.h>

FViewportPanel::FViewportPanel(
	FRenderSystem& InRenderSystem,
	FInputRouter& InInputRouter,
	const FViewportStatState& InStatState,
	FEditorSelection& InSelection)
	: ViewportWidget(InRenderSystem, InInputRouter), ViewportClient(ViewportWidget.GetViewport(), InSelection), ViewportOverlayWidget(InStatState),
	  Selection(InSelection)
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

	Toolbar.Draw(ViewportClient, [&]()
	{
		if (Toolbar.DrawCoordinateSpaceButton(ViewportClient.IsGizmoLocalSpace()))
		{
			ViewportClient.SetGizmoLocalSpace(!ViewportClient.IsGizmoLocalSpace());
		}
	});
	if (ViewportWidget.Draw(ViewportClient))
	{
		ViewportOverlayWidget.Draw();
	}
	ImGui::PopStyleVar();
	DrawContextMenu();

	ImGui::End();
}

// 짧은 Viewport 우클릭에 Node 생성 메뉴를 열고 선택한 Node를 Raycast 위치에 배치한다.
void FViewportPanel::DrawContextMenu()
{
	if (ViewportClient.ConsumeContextMenuRequest(ContextMenuPlacementLocation))
	{
		ImGui::OpenPopup("##ViewportNodeContextMenu");
	}
	if (!ImGui::BeginPopup("##ViewportNodeContextMenu"))
	{
		return;
	}

	ImGui::TextDisabled("Place Node");
	ImGui::Separator();
	if (UWorld* World = ViewportClient.GetWorld())
	{
		if (UNode* CreatedNode = FNodeCreationMenu::DrawItems(*World))
		{
			FTransform Transform = CreatedNode->GetTransform().GetRelativeTransform();
			Transform.Translation = ContextMenuPlacementLocation;
			CreatedNode->GetTransform().SetRelativeTransform(Transform);
			Selection.Select(CreatedNode);
		}
	}
	ImGui::EndPopup();
}
