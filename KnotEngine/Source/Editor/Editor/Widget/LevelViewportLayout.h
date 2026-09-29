#pragma once

#include "Core/CoreTypes.h"
#include "Core/Math/Vector2.h"

// Level Editor Viewport Panel 안에서 사용할 1~4분할 배치 종류.
enum class ELevelViewportLayout : uint8
{
	OnePane,
	TwoPanesVertical,
	TwoPanesHorizontal,
	ThreePanesLeft,
	ThreePanesRight,
	ThreePanesTop,
	ThreePanesBottom,
	FourPanesLeft,
	FourPanesRight,
	FourPanesTop,
	FourPanesBottom,
	FourPanesGrid,
	Count,
};

// ImGui 화면 좌표에서 한 Level Viewport Slot이 차지할 사각형.
struct FLevelViewportPaneRect
{
	FVector2 Position = FVector2::ZeroVector;
	FVector2 Size = FVector2::ZeroVector;
};

// 고정된 Level Viewport Slot의 배치와 사용자가 드래그하는 Splitter 비율을 관리한다.
// 레벨 수정, 카메라 조작을 담당하지 않고 UI 기능을 담당하므로 Widget에서 관리한다.
class FLevelViewportLayout final
{
public:
	FLevelViewportLayout();

	uint32 CalculatePaneRects(const FVector2& Position, const FVector2& Size, TStaticArray<FLevelViewportPaneRect, 4>& OutPaneRects);
	void DrawSplitters();
	void DrawToolbarButtons(SIZE_T SlotIndex);

	ELevelViewportLayout GetLayout() const { return Layout; }
	uint32 GetPaneCount() const;
	SIZE_T GetSlotIndex(SIZE_T PaneIndex) const;
	bool IsSlotVisible(SIZE_T SlotIndex) const;

private:
	struct FLayoutState
	{
		float Primary = 0.5f;
		float Secondary = 0.5f;
		float Tertiary = 0.5f;
	};

	struct FSplitter
	{
		FVector2 Position = FVector2::ZeroVector;
		FVector2 Size = FVector2::ZeroVector;
		float* Ratio = nullptr;
		float ParentExtent = 0.0f;
		float MinimumRatio = 0.0f;
		float MaximumRatio = 1.0f;
		bool bVertical = false;
	};

	static void SplitVertical(
		const FLevelViewportPaneRect& Rect,
		float& Ratio,
		FLevelViewportPaneRect& OutLeft,
		FLevelViewportPaneRect& OutRight,
		FSplitter& OutSplitter);
	
	static void SplitHorizontal(
		const FLevelViewportPaneRect& Rect,
		float& Ratio,
		FLevelViewportPaneRect& OutTop,
		FLevelViewportPaneRect& OutBottom,
		FSplitter& OutSplitter);

	static uint32 BuildLayout(
		ELevelViewportLayout LayoutType,
		FLayoutState& State,
		const FVector2& Position,
		const FVector2& Size,
		TStaticArray<FLevelViewportPaneRect, 4>& OutPaneRects,
		TStaticArray<FSplitter, 3>& OutSplitters,
		uint32& OutSplitterCount);

	bool DrawLayoutOption(ELevelViewportLayout LayoutType, const char* Tooltip);
	void DrawLayoutGroup(const char* Label, const ELevelViewportLayout* Layouts, const char* const* Tooltips, SIZE_T Count);
	void DrawLayoutMenu();
	void ToggleMaximize(SIZE_T SlotIndex);

	static constexpr float SplitterThickness = 5.0f;
	static constexpr float MinimumPaneSize = 48.0f;

	TStaticArray<FLayoutState, static_cast<SIZE_T>(ELevelViewportLayout::Count)> LayoutStates = {};
	TStaticArray<FSplitter, 3> Splitters = {};
	
	ELevelViewportLayout Layout = ELevelViewportLayout::OnePane;
	ELevelViewportLayout LastMultiPaneLayout = ELevelViewportLayout::FourPanesGrid;

	SIZE_T MaximizedSlotIndex = 0;
	uint32 SplitterCount = 0;
	bool bMaximized = false;
};
