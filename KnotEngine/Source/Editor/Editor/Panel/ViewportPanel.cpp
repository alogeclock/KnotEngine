#include "Editor/Panel/ViewportPanel.h"

#include "Component/TransformComponent.h"
#include "Editor/Context/EditorSelection.h"
#include "Editor/Toolbar/ViewportToolbar.h"
#include "Editor/Widget/NodeCreationMenu.h"
#include "Input/InputRouter.h"
#include "Runtime/EditorEngine.h"
#include "World/Node.h"
#include "World/World.h"

#include <algorithm>
#include <imgui.h>

// 네 Viewport Slot을 고정 생성하고 각 Slot의 기본 Camera View를 설정한다.
FViewportPanel::FViewportPanel(FRenderSystem& InRenderSystem, FInputRouter& InInputRouter, const FViewportStatState& InStatState)
	: InputRouter(InInputRouter), ViewportOverlayWidget(InStatState), Selection(GetEditor().GetEditorSelection())
{
	for (std::unique_ptr<FLevelViewportSlot>& Slot : ViewportSlots)
	{
		Slot = std::make_unique<FLevelViewportSlot>(InRenderSystem, InInputRouter);
	}

	static constexpr EEditorViewportViewMode DefaultViewModes[] = {
		EEditorViewportViewMode::Perspective,
		EEditorViewportViewMode::Top,
		EEditorViewportViewMode::Front,
		EEditorViewportViewMode::Right,
	};
	static_assert(std::size(DefaultViewModes) == 4);
	for (SIZE_T Index = 0; Index < ViewportSlots.size(); ++Index)
	{
		FLevelEditorViewportClient& ViewportClient = ViewportSlots[Index]->Client;
		ViewportClient.GetCamera().ViewMode = DefaultViewModes[Index];
		ViewportClient.OnCameraStateChanged();
	}
}

// Panel이 소유한 모든 Viewport의 입력 등록과 Render Target을 해제한다.
FViewportPanel::~FViewportPanel()
{
	for (const std::unique_ptr<FLevelViewportSlot>& Slot : ViewportSlots)
	{
		Slot->Widget.Release(Slot->Client);
	}
}

// Panel을 유지한 채 모든 Viewport의 입력 등록과 Render Target을 해제한다.
void FViewportPanel::Release()
{
	for (const std::unique_ptr<FLevelViewportSlot>& Slot : ViewportSlots)
	{
		Slot->Widget.Release(Slot->Client);
	}
}

// 수명이 고정된 네 Level Viewport Client의 non-owning 포인터를 반환한다.
TStaticArray<FLevelEditorViewportClient*, 4> FViewportPanel::GetViewportClients()
{
	TStaticArray<FLevelEditorViewportClient*, 4> ViewportClients = {};
	for (SIZE_T Index = 0; Index < ViewportSlots.size(); ++Index)
	{
		ViewportClients[Index] = &ViewportSlots[Index]->Client;
	}
	return ViewportClients;
}

// 입력 소유자와 마지막 선택을 기준으로 현재 Active Viewport Slot을 갱신한다.
void FViewportPanel::UpdateActiveViewport()
{
	IInputTarget* InputOwner = InputRouter.GetMouseCaptureOwner();
	if (!InputOwner)
	{
		InputOwner = InputRouter.GetKeyboardFocusOwner();
	}
	for (SIZE_T Index = 0; Index < ViewportSlots.size(); ++Index)
	{
		if (InputOwner == &ViewportSlots[Index]->Client)
		{
			ActiveViewportIndex = Index;
			break;
		}
	}
	if (!ViewportLayout.IsSlotVisible(ActiveViewportIndex))
	{
		ActiveViewportIndex = ViewportLayout.GetSlotIndex(0);
	}
}

// 현재 Layout의 Pane을 배치하고 보이는 Viewport만 Toolbar와 화면을 그린다.
void FViewportPanel::Draw(bool bVisible, float DeltaTime, FViewportToolbar& Toolbar)
{
	ViewportOverlayWidget.Tick(DeltaTime);
	if (!bVisible)
	{
		for (const std::unique_ptr<FLevelViewportSlot>& Slot : ViewportSlots)
		{
			Slot->Widget.Release(Slot->Client);
		}
		return;
	}

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	// 접힌 Panel은 이미지를 표시하지 않으므로 offscreen target을 해제하여 World 렌더링을 중단한다.
	if (!ImGui::Begin("Viewport"))
	{
		for (const std::unique_ptr<FLevelViewportSlot>& Slot : ViewportSlots)
		{
			Slot->Widget.Release(Slot->Client);
		}
		ImGui::End();
		ImGui::PopStyleVar();
		return;
	}

	UpdateActiveViewport();
	if (!ViewportLayout.IsSlotVisible(ActiveViewportIndex))
	{
		ActiveViewportIndex = ViewportLayout.GetSlotIndex(0);
	}

	const ImVec2 LayoutPosition = ImGui::GetCursorScreenPos();
	const ImVec2 LayoutSize = ImGui::GetContentRegionAvail();
	if (LayoutSize.x > 0.0f && LayoutSize.y > 0.0f)
	{
		TStaticArray<FLevelViewportPaneRect, 4> PaneRects = {};
		VisibleViewportCount = ViewportLayout.CalculatePaneRects(
			FVector2(LayoutPosition.x, LayoutPosition.y),
			FVector2(LayoutSize.x, LayoutSize.y),
			PaneRects);
		TStaticArray<bool, 4> bVisibleSlots = {};
		for (SIZE_T PaneIndex = 0; PaneIndex < VisibleViewportCount; ++PaneIndex)
		{
			const SIZE_T SlotIndex = ViewportLayout.GetSlotIndex(PaneIndex);
			VisibleViewportIndices[PaneIndex] = SlotIndex;
			bVisibleSlots[SlotIndex] = true;
		}
		for (SIZE_T PaneIndex = 0; PaneIndex < VisibleViewportCount; ++PaneIndex)
		{
			DrawViewportSlot(VisibleViewportIndices[PaneIndex], PaneRects[PaneIndex], Toolbar);
		}
		for (SIZE_T SlotIndex = 0; SlotIndex < ViewportSlots.size(); ++SlotIndex)
		{
			if (!bVisibleSlots[SlotIndex])
			{
				FLevelViewportSlot& Slot = *ViewportSlots[SlotIndex];
				Slot.Widget.Release(Slot.Client);
			}
		}
		ViewportLayout.DrawSplitters();
		ImGui::SetCursorScreenPos(LayoutPosition);
		ImGui::Dummy(LayoutSize);
	}
	else
	{
		VisibleViewportCount = 0;
		for (const std::unique_ptr<FLevelViewportSlot>& Slot : ViewportSlots)
		{
			Slot->Widget.Release(Slot->Client);
		}
	}
	ImGui::PopStyleVar();
	DrawContextMenu();

	ImGui::End();
}

// Layout이 계산한 한 Pane 안에 offscreen Viewport와 입력·선택 Overlay를 그린다.
void FViewportPanel::DrawViewportSlot(SIZE_T SlotIndex, const FLevelViewportPaneRect& PaneRect, FViewportToolbar& Toolbar)
{
	check(SlotIndex < ViewportSlots.size());
	FLevelViewportSlot& Slot = *ViewportSlots[SlotIndex];
	if (PaneRect.Size.X <= 0.0f || PaneRect.Size.Y <= 0.0f)
	{
		Slot.Widget.Release(Slot.Client);
		return;
	}
	ImGui::PushID(static_cast<int>(SlotIndex));
	ImGui::SetCursorScreenPos(ImVec2(PaneRect.Position.X, PaneRect.Position.Y));
	constexpr ImGuiWindowFlags SlotFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
	if (ImGui::BeginChild("##LevelViewportSlot", ImVec2(PaneRect.Size.X, PaneRect.Size.Y), ImGuiChildFlags_None, SlotFlags))
	{
		if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) &&
		    (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right)))
		{
			ActiveViewportIndex = SlotIndex;
		}
		const bool bActive = SlotIndex == ActiveViewportIndex;
		Toolbar.Draw(
			Slot.Client,
			[&]()
			{
				if (Toolbar.DrawCoordinateSpaceButton(Slot.Client.IsGizmoLocalSpace()))
				{
					Slot.Client.SetGizmoLocalSpace(!Slot.Client.IsGizmoLocalSpace());
				}
				const float LayoutButtonsWidth = ImGui::GetFrameHeight() * 2.0f + 2.0f;
				const float RightButtonsWidth = Toolbar.GetViewModeButtonsWidth() + ImGui::GetStyle().ItemSpacing.x + LayoutButtonsWidth;
				ImGui::SameLine();
				ImGui::SetCursorPosX(std::max(
					ImGui::GetCursorPosX(),
					ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x - RightButtonsWidth));
				Toolbar.DrawViewModeButtons(Slot.Client);
				ImGui::SameLine();
				ViewportLayout.DrawToolbarButtons(SlotIndex, Toolbar.GetLayoutIconsId());
			});
		const float ToolbarBottom = ImGui::GetItemRectMax().y;
		ImGui::SetCursorScreenPos(ImVec2(PaneRect.Position.X, ToolbarBottom));
		if (Slot.Widget.Draw(Slot.Client, bActive))
		{
			const ImVec2 ImageMinimum = ImGui::GetItemRectMin();
			const ImVec2 ImageMaximum = ImGui::GetItemRectMax();
			if (bActive)
			{
				ViewportOverlayWidget.Draw();
			}
			DrawBoxSelection(Slot.Client, ImageMinimum, ImageMaximum);
			if (bActive)
			{
				static constexpr ImU32 ActiveViewportColor = IM_COL32(235, 162, 10, 255);
				static constexpr float BorderInset = 1.0f;
				const ImVec2 Minimum(ImageMinimum.x + BorderInset, ImageMinimum.y + BorderInset);
				const ImVec2 Maximum(ImageMaximum.x - BorderInset, ImageMaximum.y - BorderInset);
				ImGui::GetWindowDrawList()->AddRect(Minimum, Maximum, ActiveViewportColor, 0.0f, 0, 2.0f);
			}
		}
	}
	else
	{
		Slot.Widget.Release(Slot.Client);
	}
	ImGui::EndChild();
	ImGui::PopID();
}

// 짧은 Viewport 우클릭에 Node 생성 메뉴를 열고 선택한 Node를 Raycast 위치에 배치한다.
void FViewportPanel::DrawContextMenu()
{
	for (SIZE_T PaneIndex = 0; PaneIndex < VisibleViewportCount; ++PaneIndex)
	{
		const SIZE_T SlotIndex = VisibleViewportIndices[PaneIndex];
		if (ViewportSlots[SlotIndex]->Client.ConsumeContextMenuRequest(ContextMenuPlacementLocation))
		{
			ContextMenuViewportIndex = SlotIndex;
			ImGui::OpenPopup("##ViewportNodeContextMenu");
			break;
		}
	}
	if (!ImGui::BeginPopup("##ViewportNodeContextMenu"))
	{
		return;
	}
	if (ContextMenuViewportIndex >= ViewportSlots.size())
	{
		ImGui::EndPopup();
		return;
	}

	ImGui::TextDisabled("Place Node");
	ImGui::Separator();
	if (UWorld* World = ViewportSlots[ContextMenuViewportIndex]->Client.GetWorld())
	{
		if (UNode* CreatedNode = FNodeCreationMenu::DrawItems(*World))
		{
			FEditorTransaction& EditorTransaction = GetEditor().GetEditorTransaction();
			EditorTransaction.Begin(FName("Add Node"));
			EditorTransaction.TrackNode(*CreatedNode);
			FTransform Transform = CreatedNode->GetTransform().GetRelativeTransform();
			Transform.Translation = ContextMenuPlacementLocation;
			CreatedNode->GetTransform().SetRelativeTransform(Transform);
			EditorTransaction.End();
			Selection.Select(CreatedNode);
		}
	}
	ImGui::EndPopup();
}

// Viewport 영역으로 Clip한 반투명 박스 선택 사각형을 표시한다.
void FViewportPanel::DrawBoxSelection(const FLevelEditorViewportClient& ViewportClient, const ImVec2& ImageMinimum, const ImVec2& ImageMaximum) const
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
