#pragma once

#include "Input/InputEvents.h"

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
	EModifierKeyMask GetModifiers() const { return Modifiers; }
	const TArray<FInputEvent>& GetEvents() const { return Events; }

private:
	friend class FWindowsInput;

	uint64 FrameNumber = 0;

	static constexpr SIZE_T KeyCount = static_cast<SIZE_T>(EKeyboardKey::Count);
	TStaticArray<bool, KeyCount> KeysDown = {};
	TStaticArray<bool, KeyCount> KeysPressed = {};
	TStaticArray<bool, KeyCount> KeysReleased = {};
	EModifierKeyMask Modifiers = EModifierKeyMask::None;

	EMouseButtonMask MouseButtonsDown = EMouseButtonMask::None;
	EMouseButtonMask MouseButtonsPressed = EMouseButtonMask::None;
	EMouseButtonMask MouseButtonsReleased = EMouseButtonMask::None;

	FVector2 PointerPosition = FVector2::ZeroVector;
	FVector2 PointerDelta = FVector2::ZeroVector;
	FVector2 RawPointerDelta = FVector2::ZeroVector;
	FVector2 WheelDelta = FVector2::ZeroVector;

	TArray<FInputEvent> Events;

	bool bHasPointerPosition = false;
	bool bHasFocus = false;
};
