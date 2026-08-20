#pragma once

#include "Input/InputTypes.h"

class FWindowsInput;

// 한 프레임 동안 수집된 입력 상태와 순서 보존 이벤트를 읽기 전용으로 제공한다.
class FInputSnapshot
{
public:
	uint64 GetFrameNumber() const { return FrameNumber; }
	bool HasFocus() const { return bHasFocus; }

	bool IsKeyDown(EKeyboardKey Key) const;
	bool WasKeyPressed(EKeyboardKey Key) const;
	bool WasKeyReleased(EKeyboardKey Key) const;

	bool IsMouseButtonDown(EMouseButton Button) const;
	bool WasMouseButtonPressed(EMouseButton Button) const;
	bool WasMouseButtonReleased(EMouseButton Button) const;

	bool HasPointerPosition() const { return bHasPointerPosition; }
	const FVector2& GetPointerPosition() const { return PointerPosition; }
	const FVector2& GetPointerDelta() const { return PointerDelta; }
	const FVector2& GetRawPointerDelta() const { return RawPointerDelta; }
	const FVector2& GetWheelDelta() const { return WheelDelta; }
	EInputModifier GetModifiers() const { return Modifiers; }
	const TArray<FInputEvent>& GetEvents() const { return Events; }

private:
	friend class FWindowsInput;

	static constexpr SIZE_T KeyCount = static_cast<SIZE_T>(EKeyboardKey::Count);
	static constexpr SIZE_T MouseButtonCount = static_cast<SIZE_T>(EMouseButton::Count);

	uint64 FrameNumber = 0;

	TStaticArray<bool, KeyCount> KeysDown = {};
	TStaticArray<bool, KeyCount> KeysPressed = {};
	TStaticArray<bool, KeyCount> KeysReleased = {};

	TStaticArray<bool, MouseButtonCount> MouseButtonsDown = {};
	TStaticArray<bool, MouseButtonCount> MouseButtonsPressed = {};
	TStaticArray<bool, MouseButtonCount> MouseButtonsReleased = {};

	FVector2 PointerPosition = FVector2::ZeroVector;
	FVector2 PointerDelta = FVector2::ZeroVector;
	FVector2 RawPointerDelta = FVector2::ZeroVector;
	FVector2 WheelDelta = FVector2::ZeroVector;

	EInputModifier Modifiers = EInputModifier::None;
	TArray<FInputEvent> Events;

	bool bHasPointerPosition = false;
	bool bHasFocus = false;
};
