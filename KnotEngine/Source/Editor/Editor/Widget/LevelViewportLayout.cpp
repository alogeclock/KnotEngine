#include "Editor/Widget/LevelViewportLayout.h"

#include "Core/Assert.h"

#include <algorithm>
#include <cmath>
#include <imgui.h>

// 비대칭 4분할 Layout의 보조 Splitter 비율을 기본값으로 초기화한다.
FLevelViewportLayout::FLevelViewportLayout()
{
	for (ELevelViewportLayout LayoutType : {
		     ELevelViewportLayout::FourPanesLeft,
		     ELevelViewportLayout::FourPanesRight,
		     ELevelViewportLayout::FourPanesTop,
		     ELevelViewportLayout::FourPanesBottom })
	{
		FLayoutState& State = LayoutStates[static_cast<SIZE_T>(LayoutType)];
		State.Secondary = 1.0f / 3.0f;
		State.Tertiary = 0.5f;
	}
}

// 최대화 상태를 포함하여 현재 화면에 표시할 Pane 수를 반환한다.
uint32 FLevelViewportLayout::GetPaneCount() const
{
	if (bMaximized)
	{
		return 1;
	}
	switch (Layout)
	{
	case ELevelViewportLayout::OnePane:
		return 1;
	case ELevelViewportLayout::TwoPanesVertical:
	case ELevelViewportLayout::TwoPanesHorizontal:
		return 2;
	case ELevelViewportLayout::ThreePanesLeft:
	case ELevelViewportLayout::ThreePanesRight:
	case ELevelViewportLayout::ThreePanesTop:
	case ELevelViewportLayout::ThreePanesBottom:
		return 3;
	case ELevelViewportLayout::FourPanesLeft:
	case ELevelViewportLayout::FourPanesRight:
	case ELevelViewportLayout::FourPanesTop:
	case ELevelViewportLayout::FourPanesBottom:
	case ELevelViewportLayout::FourPanesGrid:
		return 4;
	case ELevelViewportLayout::Count:
		break;
	}
	return 1;
}

// 표시 순서의 Pane Index를 수명이 고정된 Viewport Slot Index로 변환한다.
SIZE_T FLevelViewportLayout::GetSlotIndex(SIZE_T PaneIndex) const
{
	check(PaneIndex < GetPaneCount());
	return bMaximized ? MaximizedSlotIndex : PaneIndex;
}

// 지정한 Viewport Slot이 현재 Layout 또는 최대화 화면에 포함되는지 확인한다.
bool FLevelViewportLayout::IsSlotVisible(SIZE_T SlotIndex) const
{
	for (SIZE_T PaneIndex = 0; PaneIndex < GetPaneCount(); ++PaneIndex)
	{
		if (GetSlotIndex(PaneIndex) == SlotIndex)
		{
			return true;
		}
	}
	return false;
}

// 현재 Layout과 Splitter 비율로 이번 Frame의 Pane 사각형을 계산한다.
uint32 FLevelViewportLayout::CalculatePaneRects(const FVector2& Position, const FVector2& Size, TStaticArray<FLevelViewportPaneRect, 4>& OutPaneRects)
{
	if (bMaximized)
	{
		SplitterCount = 0;
		OutPaneRects[0] = { Position, Size };
		return 1;
	}
	FLayoutState& State = LayoutStates[static_cast<SIZE_T>(Layout)];
	return BuildLayout(Layout, State, Position, Size, OutPaneRects, Splitters, SplitterCount);
}

// 주어진 영역을 좌우 Pane으로 나누고 수직 Splitter 정보를 기록한다.
void FLevelViewportLayout::SplitVertical(
	const FLevelViewportPaneRect& Rect,
	float& Ratio,
	FLevelViewportPaneRect& OutLeft,
	FLevelViewportPaneRect& OutRight,
	FSplitter& OutSplitter)
{
	const float AppliedThickness = std::min(SplitterThickness, std::max(0.0f, Rect.Size.X));
	const float HalfThickness = AppliedThickness * 0.5f;
	const float MaximumMinimumSize = std::max(0.0f, (Rect.Size.X - AppliedThickness) * 0.5f);
	const float MinimumSize = std::min(MinimumPaneSize, MaximumMinimumSize);
	const float MinimumRatio = Rect.Size.X > 0.0f ? (MinimumSize + HalfThickness) / Rect.Size.X : 0.5f;
	const float MaximumRatio = 1.0f - MinimumRatio;
	Ratio = std::clamp(Ratio, MinimumRatio, MaximumRatio);
	const float SplitPosition = Rect.Size.X * Ratio;
	const float SplitterMinimum = std::round(Rect.Position.X + SplitPosition - HalfThickness);
	const float LocalMinimum = std::clamp(SplitterMinimum - Rect.Position.X, 0.0f, Rect.Size.X - AppliedThickness);
	const float LocalMaximum = LocalMinimum + AppliedThickness;

	OutLeft = { Rect.Position, FVector2(LocalMinimum, Rect.Size.Y) };
	OutRight = {
		FVector2(Rect.Position.X + LocalMaximum, Rect.Position.Y),
		FVector2(std::max(0.0f, Rect.Size.X - LocalMaximum), Rect.Size.Y)
	};
	OutSplitter = {
		FVector2(Rect.Position.X + LocalMinimum, Rect.Position.Y),
		FVector2(AppliedThickness, Rect.Size.Y),
		&Ratio,
		Rect.Size.X,
		MinimumRatio,
		MaximumRatio,
		true
	};
}

// 주어진 영역을 상하 Pane으로 나누고 수평 Splitter 정보를 기록한다.
void FLevelViewportLayout::SplitHorizontal(
	const FLevelViewportPaneRect& Rect,
	float& Ratio,
	FLevelViewportPaneRect& OutTop,
	FLevelViewportPaneRect& OutBottom,
	FSplitter& OutSplitter)
{
	const float AppliedThickness = std::min(SplitterThickness, std::max(0.0f, Rect.Size.Y));
	const float HalfThickness = AppliedThickness * 0.5f;
	const float MaximumMinimumSize = std::max(0.0f, (Rect.Size.Y - AppliedThickness) * 0.5f);
	const float MinimumSize = std::min(MinimumPaneSize, MaximumMinimumSize);
	const float MinimumRatio = Rect.Size.Y > 0.0f ? (MinimumSize + HalfThickness) / Rect.Size.Y : 0.5f;
	const float MaximumRatio = 1.0f - MinimumRatio;
	Ratio = std::clamp(Ratio, MinimumRatio, MaximumRatio);
	const float SplitPosition = Rect.Size.Y * Ratio;
	const float SplitterMinimum = std::round(Rect.Position.Y + SplitPosition - HalfThickness);
	const float LocalMinimum = std::clamp(SplitterMinimum - Rect.Position.Y, 0.0f, Rect.Size.Y - AppliedThickness);
	const float LocalMaximum = LocalMinimum + AppliedThickness;

	OutTop = { Rect.Position, FVector2(Rect.Size.X, LocalMinimum) };
	OutBottom = {
		FVector2(Rect.Position.X, Rect.Position.Y + LocalMaximum),
		FVector2(Rect.Size.X, std::max(0.0f, Rect.Size.Y - LocalMaximum))
	};
	OutSplitter = {
		FVector2(Rect.Position.X, Rect.Position.Y + LocalMinimum),
		FVector2(Rect.Size.X, AppliedThickness),
		&Ratio,
		Rect.Size.Y,
		MinimumRatio,
		MaximumRatio,
		false
	};
}

// 선택한 Layout의 중첩 분할 순서에 따라 Pane과 Splitter를 구성한다.
uint32 FLevelViewportLayout::BuildLayout(
	ELevelViewportLayout LayoutType,
	FLayoutState& State,
	const FVector2& Position,
	const FVector2& Size,
	TStaticArray<FLevelViewportPaneRect, 4>& OutPaneRects,
	TStaticArray<FSplitter, 3>& OutSplitters,
	uint32& OutSplitterCount)
{
	const FLevelViewportPaneRect FullRect = { Position, Size };
	FLevelViewportPaneRect First;
	FLevelViewportPaneRect Second;
	FLevelViewportPaneRect Remainder;
	OutSplitterCount = 0;

	switch (LayoutType)
	{
	case ELevelViewportLayout::OnePane:
		OutPaneRects[0] = FullRect;
		return 1;
	case ELevelViewportLayout::TwoPanesVertical:
		SplitVertical(FullRect, State.Primary, OutPaneRects[0], OutPaneRects[1], OutSplitters[OutSplitterCount++]);
		return 2;
	case ELevelViewportLayout::TwoPanesHorizontal:
		SplitHorizontal(FullRect, State.Primary, OutPaneRects[0], OutPaneRects[1], OutSplitters[OutSplitterCount++]);
		return 2;
	case ELevelViewportLayout::ThreePanesLeft:
		SplitVertical(FullRect, State.Primary, OutPaneRects[0], Remainder, OutSplitters[OutSplitterCount++]);
		SplitHorizontal(Remainder, State.Secondary, OutPaneRects[1], OutPaneRects[2], OutSplitters[OutSplitterCount++]);
		return 3;
	case ELevelViewportLayout::ThreePanesRight:
		SplitVertical(FullRect, State.Primary, Remainder, OutPaneRects[0], OutSplitters[OutSplitterCount++]);
		SplitHorizontal(Remainder, State.Secondary, OutPaneRects[1], OutPaneRects[2], OutSplitters[OutSplitterCount++]);
		return 3;
	case ELevelViewportLayout::ThreePanesTop:
		SplitHorizontal(FullRect, State.Primary, OutPaneRects[0], Remainder, OutSplitters[OutSplitterCount++]);
		SplitVertical(Remainder, State.Secondary, OutPaneRects[1], OutPaneRects[2], OutSplitters[OutSplitterCount++]);
		return 3;
	case ELevelViewportLayout::ThreePanesBottom:
		SplitHorizontal(FullRect, State.Primary, Remainder, OutPaneRects[0], OutSplitters[OutSplitterCount++]);
		SplitVertical(Remainder, State.Secondary, OutPaneRects[1], OutPaneRects[2], OutSplitters[OutSplitterCount++]);
		return 3;
	case ELevelViewportLayout::FourPanesLeft:
		SplitVertical(FullRect, State.Primary, OutPaneRects[0], Remainder, OutSplitters[OutSplitterCount++]);
		SplitHorizontal(Remainder, State.Secondary, OutPaneRects[1], Remainder, OutSplitters[OutSplitterCount++]);
		SplitHorizontal(Remainder, State.Tertiary, OutPaneRects[2], OutPaneRects[3], OutSplitters[OutSplitterCount++]);
		return 4;
	case ELevelViewportLayout::FourPanesRight:
		SplitVertical(FullRect, State.Primary, Remainder, OutPaneRects[0], OutSplitters[OutSplitterCount++]);
		SplitHorizontal(Remainder, State.Secondary, OutPaneRects[1], Remainder, OutSplitters[OutSplitterCount++]);
		SplitHorizontal(Remainder, State.Tertiary, OutPaneRects[2], OutPaneRects[3], OutSplitters[OutSplitterCount++]);
		return 4;
	case ELevelViewportLayout::FourPanesTop:
		SplitHorizontal(FullRect, State.Primary, OutPaneRects[0], Remainder, OutSplitters[OutSplitterCount++]);
		SplitVertical(Remainder, State.Secondary, OutPaneRects[1], Remainder, OutSplitters[OutSplitterCount++]);
		SplitVertical(Remainder, State.Tertiary, OutPaneRects[2], OutPaneRects[3], OutSplitters[OutSplitterCount++]);
		return 4;
	case ELevelViewportLayout::FourPanesBottom:
		SplitHorizontal(FullRect, State.Primary, Remainder, OutPaneRects[0], OutSplitters[OutSplitterCount++]);
		SplitVertical(Remainder, State.Secondary, OutPaneRects[1], Remainder, OutSplitters[OutSplitterCount++]);
		SplitVertical(Remainder, State.Tertiary, OutPaneRects[2], OutPaneRects[3], OutSplitters[OutSplitterCount++]);
		return 4;
	case ELevelViewportLayout::FourPanesGrid:
		SplitVertical(FullRect, State.Primary, First, Second, OutSplitters[OutSplitterCount++]);
		SplitHorizontal(First, State.Secondary, OutPaneRects[0], OutPaneRects[2], OutSplitters[OutSplitterCount++]);
		SplitHorizontal(Second, State.Secondary, OutPaneRects[1], OutPaneRects[3], OutSplitters[OutSplitterCount++]);
		// 두 열이 같은 수평 비율을 공유하므로 겹치는 두 Splitter를 하나의 전체 폭 Splitter로 합친다.
		OutSplitters[1].Position.X = FullRect.Position.X;
		OutSplitters[1].Size.X = FullRect.Size.X;
		OutSplitterCount = 2;
		return 4;
	case ELevelViewportLayout::Count:
		break;
	}

	OutPaneRects[0] = FullRect;
	return 1;
}

// 계산된 Splitter를 그리고 Drag 입력으로 해당 Layout의 분할 비율을 갱신한다.
void FLevelViewportLayout::DrawSplitters()
{
	for (uint32 Index = 0; Index < SplitterCount; ++Index)
	{
		FSplitter& Splitter = Splitters[Index];
		if (!Splitter.Ratio || Splitter.ParentExtent <= 0.0f || Splitter.Size.X <= 0.0f || Splitter.Size.Y <= 0.0f)
		{
			continue;
		}

		ImGui::PushID(static_cast<int>(Index));
		ImGui::SetCursorScreenPos(ImVec2(Splitter.Position.X, Splitter.Position.Y));
		ImGui::InvisibleButton("##LevelViewportSplitter", ImVec2(Splitter.Size.X, Splitter.Size.Y));
		const bool bHovered = ImGui::IsItemHovered();
		const bool bActive = ImGui::IsItemActive();
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		const ImVec2 Minimum(Splitter.Position.X, Splitter.Position.Y);
		const ImVec2 Maximum(Splitter.Position.X + Splitter.Size.X, Splitter.Position.Y + Splitter.Size.Y);
		static constexpr ImU32 BorderColor = IM_COL32(18, 18, 18, 255);
		static constexpr ImU32 BackgroundColor = IM_COL32(38, 38, 38, 255);
		const ImU32 CenterColor = bActive ? IM_COL32(200, 200, 200, 255) : bHovered ? IM_COL32(140, 140, 140, 255) : IM_COL32(76, 76, 76, 255);
		DrawList->AddRectFilled(Minimum, Maximum, BorderColor);
		if (Splitter.Size.X > 2.0f && Splitter.Size.Y > 2.0f)
		{
			DrawList->AddRectFilled(ImVec2(Minimum.x + 1.0f, Minimum.y + 1.0f), ImVec2(Maximum.x - 1.0f, Maximum.y - 1.0f), BackgroundColor);
		}
		if (Splitter.bVertical)
		{
			const float Center = (Minimum.x + Maximum.x) * 0.5f;
			DrawList->AddLine(ImVec2(Center, Minimum.y + 1.0f), ImVec2(Center, Maximum.y - 1.0f), CenterColor, bActive ? 2.0f : 1.0f);
		}
		else
		{
			const float Center = (Minimum.y + Maximum.y) * 0.5f;
			DrawList->AddLine(ImVec2(Minimum.x + 1.0f, Center), ImVec2(Maximum.x - 1.0f, Center), CenterColor, bActive ? 2.0f : 1.0f);
		}
		if (bHovered || bActive)
		{
			ImGui::SetMouseCursor(Splitter.bVertical ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);
		}
		if (bActive)
		{
			const ImVec2 MouseDelta = ImGui::GetIO().MouseDelta;
			const float Delta = Splitter.bVertical ? MouseDelta.x : MouseDelta.y;
			*Splitter.Ratio = std::clamp(*Splitter.Ratio + Delta / Splitter.ParentExtent, Splitter.MinimumRatio, Splitter.MaximumRatio);
		}
		ImGui::PopID();
	}
}

// Layout 선택 버튼에 축소 배치를 그려 클릭 시 해당 Layout으로 전환한다.
bool FLevelViewportLayout::DrawLayoutOption(ELevelViewportLayout LayoutType, const char* Tooltip)
{
	ImGui::PushID(static_cast<int>(LayoutType));
	const bool bSelected = Layout == LayoutType;
	if (bSelected)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
	}
	const bool bClicked = ImGui::Button("##Layout", ImVec2(48.0f, 36.0f));
	if (bSelected)
	{
		ImGui::PopStyleColor();
	}

	const ImVec2 Minimum = ImGui::GetItemRectMin();
	const ImVec2 Maximum = ImGui::GetItemRectMax();
	FLayoutState PreviewState;
	if (LayoutType == ELevelViewportLayout::FourPanesLeft || LayoutType == ELevelViewportLayout::FourPanesRight ||
	    LayoutType == ELevelViewportLayout::FourPanesTop || LayoutType == ELevelViewportLayout::FourPanesBottom)
	{
		PreviewState.Secondary = 1.0f / 3.0f;
	}
	TStaticArray<FLevelViewportPaneRect, 4> PreviewPanes = {};
	TStaticArray<FSplitter, 3> PreviewSplitters = {};
	uint32 PreviewSplitterCount = 0;
	const uint32 PaneCount = BuildLayout(
		LayoutType,
		PreviewState,
		FVector2(Minimum.x + 5.0f, Minimum.y + 5.0f),
		FVector2(Maximum.x - Minimum.x - 10.0f, Maximum.y - Minimum.y - 10.0f),
		PreviewPanes,
		PreviewSplitters,
		PreviewSplitterCount);
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const ImU32 PaneColor = ImGui::GetColorU32(ImGuiCol_TextDisabled);
	for (uint32 Index = 0; Index < PaneCount; ++Index)
	{
		const FLevelViewportPaneRect& Pane = PreviewPanes[Index];
		DrawList->AddRect(ImVec2(Pane.Position.X, Pane.Position.Y), ImVec2(Pane.Position.X + Pane.Size.X, Pane.Position.Y + Pane.Size.Y), PaneColor);
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("%s", Tooltip);
	}
	if (bClicked)
	{
		bMaximized = false;
		Layout = LayoutType;
		if (LayoutType != ELevelViewportLayout::OnePane)
		{
			LastMultiPaneLayout = LayoutType;
		}
		ImGui::CloseCurrentPopup();
	}
	ImGui::PopID();
	return bClicked;
}

// 같은 Pane 수를 가진 Layout 선택 버튼들을 Popup의 한 그룹으로 그린다.
void FLevelViewportLayout::DrawLayoutGroup(const char* Label, const ELevelViewportLayout* Layouts, const char* const* Tooltips, SIZE_T Count)
{
	ImGui::TextDisabled("%s", Label);
	ImGui::Separator();
	for (SIZE_T Index = 0; Index < Count; ++Index)
	{
		if (Index != 0)
		{
			ImGui::SameLine();
		}
		DrawLayoutOption(Layouts[Index], Tooltips[Index]);
	}
	ImGui::Spacing();
}

// 지원하는 1~4분할 Layout 선택지를 그룹별 Popup으로 그린다.
void FLevelViewportLayout::DrawLayoutMenu()
{
	if (!ImGui::BeginPopup("##LevelViewportLayoutMenu"))
	{
		return;
	}

	static constexpr ELevelViewportLayout OnePaneLayouts[] = { ELevelViewportLayout::OnePane };
	static constexpr const char* OnePaneTooltips[] = { "One Pane" };
	static constexpr ELevelViewportLayout TwoPaneLayouts[] = {
		ELevelViewportLayout::TwoPanesVertical,
		ELevelViewportLayout::TwoPanesHorizontal,
	};
	static constexpr const char* TwoPaneTooltips[] = { "Two Panes Vertical", "Two Panes Horizontal" };
	static constexpr ELevelViewportLayout ThreePaneLayouts[] = {
		ELevelViewportLayout::ThreePanesLeft,
		ELevelViewportLayout::ThreePanesRight,
		ELevelViewportLayout::ThreePanesTop,
		ELevelViewportLayout::ThreePanesBottom,
	};
	static constexpr const char* ThreePaneTooltips[] = {
		"Three Panes Left",
		"Three Panes Right",
		"Three Panes Top",
		"Three Panes Bottom",
	};
	static constexpr ELevelViewportLayout FourPaneLayouts[] = {
		ELevelViewportLayout::FourPanesLeft,
		ELevelViewportLayout::FourPanesRight,
		ELevelViewportLayout::FourPanesTop,
		ELevelViewportLayout::FourPanesBottom,
		ELevelViewportLayout::FourPanesGrid,
	};
	static constexpr const char* FourPaneTooltips[] = {
		"Four Panes Left",
		"Four Panes Right",
		"Four Panes Top",
		"Four Panes Bottom",
		"Four Panes Grid",
	};

	DrawLayoutGroup("ONE PANE", OnePaneLayouts, OnePaneTooltips, std::size(OnePaneLayouts));
	DrawLayoutGroup("TWO PANES", TwoPaneLayouts, TwoPaneTooltips, std::size(TwoPaneLayouts));
	DrawLayoutGroup("THREE PANES", ThreePaneLayouts, ThreePaneTooltips, std::size(ThreePaneLayouts));
	DrawLayoutGroup("FOUR PANES", FourPaneLayouts, FourPaneTooltips, std::size(FourPaneLayouts));
	ImGui::EndPopup();
}

// 지정한 Slot의 최대화와 직전 다중 Pane Layout 복원을 전환한다.
void FLevelViewportLayout::ToggleMaximize(SIZE_T SlotIndex)
{
	check(SlotIndex < 4);
	if (bMaximized)
	{
		bMaximized = false;
		return;
	}
	if (Layout == ELevelViewportLayout::OnePane)
	{
		Layout = LastMultiPaneLayout;
		return;
	}

	LastMultiPaneLayout = Layout;
	MaximizedSlotIndex = SlotIndex;
	bMaximized = true;
}

// Toolbar에 Layout Popup 버튼과 최대화·복원 상태 아이콘을 그린다.
void FLevelViewportLayout::DrawToolbarButtons(SIZE_T SlotIndex)
{
	const float ButtonSize = ImGui::GetFrameHeight();
	if (ImGui::Button("##LevelViewportLayoutMenuButton", ImVec2(ButtonSize, ButtonSize)))
	{
		ImGui::OpenPopup("##LevelViewportLayoutMenu");
	}
	const ImVec2 MenuMinimum = ImGui::GetItemRectMin();
	const ImVec2 MenuMaximum = ImGui::GetItemRectMax();
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const ImU32 IconColor = ImGui::GetColorU32(ImGuiCol_Text);
	const float MenuCenterX = (MenuMinimum.x + MenuMaximum.x) * 0.5f;
	const float MenuCenterY = (MenuMinimum.y + MenuMaximum.y) * 0.5f;
	for (int Offset = -1; Offset <= 1; ++Offset)
	{
		DrawList->AddCircleFilled(ImVec2(MenuCenterX, MenuCenterY + static_cast<float>(Offset) * 4.0f), 1.25f, IconColor);
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Viewport Layout");
	}

	ImGui::SameLine(0.0f, 2.0f);
	const bool bSinglePane = bMaximized || Layout == ELevelViewportLayout::OnePane;
	if (ImGui::Button("##LevelViewportMaximizeButton", ImVec2(ButtonSize, ButtonSize)))
	{
		ToggleMaximize(SlotIndex);
	}
	const ImVec2 ToggleMinimum = ImGui::GetItemRectMin();
	const ImVec2 ToggleMaximum = ImGui::GetItemRectMax();
	const float Padding = 5.0f;
	const ImVec2 IconMinimum(ToggleMinimum.x + Padding, ToggleMinimum.y + Padding);
	const ImVec2 IconMaximum(ToggleMaximum.x - Padding, ToggleMaximum.y - Padding);
	if (bSinglePane)
	{
		const float CenterX = (IconMinimum.x + IconMaximum.x) * 0.5f;
		const float CenterY = (IconMinimum.y + IconMaximum.y) * 0.5f;
		DrawList->AddRect(IconMinimum, IconMaximum, IconColor, 2.0f);
		DrawList->AddLine(ImVec2(CenterX, IconMinimum.y), ImVec2(CenterX, IconMaximum.y), IconColor);
		DrawList->AddLine(ImVec2(IconMinimum.x, CenterY), ImVec2(IconMaximum.x, CenterY), IconColor);
	}
	else
	{
		DrawList->AddRect(IconMinimum, IconMaximum, IconColor, 2.0f);
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("%s", bSinglePane ? "Restore Viewports" : "Maximize Viewport");
	}
	ImGui::SetNextWindowPos(ImVec2(MenuMinimum.x, MenuMaximum.y), ImGuiCond_Appearing);
	DrawLayoutMenu();
}
