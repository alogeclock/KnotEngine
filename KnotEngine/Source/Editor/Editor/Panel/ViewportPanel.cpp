#include "Editor/Panel/ViewportPanel.h"

#include "Component/TransformComponent.h"
#include "Editor/EditorSelection.h"
#include "Editor/Toolbar/ViewportToolbar.h"
#include "Editor/Widget/NodeCreationMenu.h"
#include "Runtime/EditorEngine.h"
#include "World/Node.h"
#include "World/World.h"

#include <algorithm>
#include <imgui.h>

FViewportPanel::FViewportPanel(
	FRenderSystem& InRenderSystem, FInputRouter& InInputRouter, const FViewportStatState& InStatState,
	FEditorSelection& InSelection, UEditorEngine& InEditorEngine)
	: ViewportWidget(InRenderSystem, InInputRouter), ViewportClient(ViewportWidget.GetViewport(), InEditorEngine),
	  ViewportOverlayWidget(InStatState), Selection(InSelection), EditorEngine(InEditorEngine)
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
		const ImVec2 ImageMinimum = ImGui::GetItemRectMin();
		const ImVec2 ImageMaximum = ImGui::GetItemRectMax();
		ViewportOverlayWidget.Draw();
		DrawBoxSelection(ImageMinimum, ImageMaximum);
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
			FTransactionManager& TransactionManager = EditorEngine.GetTransactionManager();
			TransactionManager.Begin(FName("Add Node"));
			TransactionManager.TrackNode(*CreatedNode);
			FTransform Transform = CreatedNode->GetTransform().GetRelativeTransform();
			Transform.Translation = ContextMenuPlacementLocation;
			CreatedNode->GetTransform().SetRelativeTransform(Transform);
			TransactionManager.End();
			Selection.Select(CreatedNode);
		}
	}
	ImGui::EndPopup();
}

// Viewport 영역으로 Clip한 반투명 박스 선택 사각형을 표시한다.
void FViewportPanel::DrawBoxSelection(const ImVec2& ImageMinimum, const ImVec2& ImageMaximum) const
{
	FVector2 Start;
	FVector2 End;
	bool bAdditive = false;
	if (!ViewportClient.GetBoxSelection(Start, End, bAdditive))
	{
		return;
	}

	const ImVec2 Minimum(std::min(Start.X, End.X), std::min(Start.Y, End.Y));
	const ImVec2 Maximum(std::max(Start.X, End.X), std::max(Start.Y, End.Y));
	const ImU32 OutlineColor = bAdditive ? IM_COL32(128, 240, 128, 220) : IM_COL32(128, 192, 255, 220);
	const ImU32 FillColor = bAdditive ? IM_COL32(64, 180, 64, 40) : IM_COL32(64, 128, 220, 40);
	ImDrawList* DrawList = ImGui::GetForegroundDrawList();
	DrawList->PushClipRect(ImageMinimum, ImageMaximum, true);
	DrawList->AddRectFilled(Minimum, Maximum, FillColor);
	DrawList->AddRect(Minimum, Maximum, OutlineColor, 0.0f, 0, 1.5f);
	DrawList->PopClipRect();
}
