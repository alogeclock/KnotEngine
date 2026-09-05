#pragma once

#include "EngineAPI.h"

#include "Input/InputSnapshot.h"

#include <Windows.h>

// Win32 입력 메시지를 엔진 입력 상태와 프레임 스냅샷으로 변환한다.
// 수명은 프로세스 범위이며 Startup/Shutdown은 각각 한 번만 호출한다.
// Shutdown은 누적된 입력 상태를 초기화하지 않으므로 같은 인스턴스를 다시 Startup할 수 없다.
class ENGINE_API FWindowsInput final
{
public:
	void Startup(HWND InWindowHandle);
	void Shutdown();
	void ProcessMessage(HWND MessageWindowHandle, UINT Message, WPARAM WParam, LPARAM LParam);

	FInputSnapshot TakeSnapshot();

private:
	static constexpr SIZE_T KeyCount = static_cast<SIZE_T>(EKeyboardKey::Count);
	static constexpr SIZE_T MouseButtonCount = static_cast<SIZE_T>(EMouseButton::Count);

	// 키 메시지 LParam의 비트 플래그.
	static constexpr LPARAM ExtendedKeyMask = 1LL << 24;
	static constexpr LPARAM PreviousKeyStateMask = 1LL << 30;

	static EKeyboardKey TranslateVirtualKey(WPARAM WParam, LPARAM LParam);
	static EMouseButton TranslateXButton(WPARAM WParam);

	void ProcessFocusChange(bool bInHasFocus);
	void ProcessKeyDown(WPARAM WParam, LPARAM LParam);
	void ProcessKeyUp(WPARAM WParam, LPARAM LParam);
	void ProcessUtf32Character(WPARAM WParam, uint16 RepeatCount);
	void ProcessUtf16Character(char16_t CodeUnit, uint16 RepeatCount);
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
	void AddCharacter(char32_t Character, uint16 RepeatCount);
	void SetWindowFocusState(bool bInHasFocus);
	void ReleaseMouseButtons(bool bEmitReleaseEvents);
	void ReleaseMouseCaptureIfUnused();
	void ProcessRawInput(LPARAM LParam);

	EModifierKeyMask GetModifiers() const;

	HWND WindowHandle = nullptr;

	TStaticArray<bool, KeyCount> KeysDown = {};
	TStaticArray<bool, KeyCount> KeysPressed = {};
	TStaticArray<bool, KeyCount> KeysReleased = {};

	EMouseButtonMask MouseButtonsDown = EMouseButtonMask::None;
	EMouseButtonMask MouseButtonsPressed = EMouseButtonMask::None;
	EMouseButtonMask MouseButtonsReleased = EMouseButtonMask::None;

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
