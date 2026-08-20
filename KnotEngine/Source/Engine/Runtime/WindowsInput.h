#pragma once

#include "Input/InputSnapshot.h"

#include <Windows.h>

// Win32 입력 메시지를 엔진 입력 상태와 프레임 스냅샷으로 변환한다.
class FWindowsInput final
{
public:
	void Startup(HWND InWindowHandle);
	void Shutdown();
	void ProcessMessage(HWND MessageWindowHandle, UINT Message, WPARAM WParam, LPARAM LParam);

	FInputSnapshot TakeSnapshot();

private:
	static constexpr SIZE_T KeyCount = static_cast<SIZE_T>(EKeyboardKey::Count);
	static constexpr SIZE_T MouseButtonCount = static_cast<SIZE_T>(EMouseButton::Count);

	static EKeyboardKey TranslateVirtualKey(WPARAM WParam, LPARAM LParam);
	static EMouseButton TranslateXButton(WPARAM WParam);
	static EMouseButtonMask GetMouseButtonMask(EMouseButton Button);

	void ProcessFocusChange(bool bInHasFocus);
	void ProcessKeyDown(WPARAM WParam, LPARAM LParam);
	void ProcessKeyUp(WPARAM WParam, LPARAM LParam);
	void ProcessUtf32Character(WPARAM WParam);
	void ProcessMouseMove(LPARAM LParam);
	void ProcessMouseButtonDown(EMouseButton Button, bool bDoubleClick, LPARAM LParam);
	void ProcessMouseButtonUp(EMouseButton Button, LPARAM LParam);
	void ProcessMouseWheel(UINT Message, WPARAM WParam, LPARAM LParam);
	void ProcessCaptureChanged(LPARAM LParam);

	void SetKeyState(EKeyboardKey Key, bool bDown, bool bRepeat = false);
	void SetMouseButtonState(EMouseButton Button, bool bDown, bool bDoubleClick = false);
	void SetPointerPosition(const FVector2& InPointerPosition);
	void AddRawPointerDelta(const FVector2& InRawPointerDelta);
	void AddWheelDelta(const FVector2& InWheelDelta);
	void AddCharacter(char32_t Character);
	void SetWindowFocusState(bool bInHasFocus);
	void ReleaseMouseButtons(bool bEmitReleaseEvents);
	void ReleaseMouseCaptureIfUnused();
	void ProcessRawInput(LPARAM LParam);
	void ProcessUtf16Character(char16_t Character);

	bool HasMouseButtonDown() const;
	EInputModifier GetModifiers() const;
	EMouseButtonMask GetPressedMouseButtons() const;

	HWND WindowHandle = nullptr;

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

	TArray<FInputEvent> PendingEvents;

	uint64 NextFrameNumber = 1;
	char16_t PendingHighSurrogate = 0;
	bool bHasPointerPosition = false;
	bool bHasFocus = false;
};
