#include "Platform/WindowsInput.h"

#include "Core/Assert.h"
#include "Core/Log.h"

#include <hidusage.h>
#include <windowsx.h>

#include <utility>

void FWindowsInput::Startup(HWND InWindowHandle)
{
	check(!WindowHandle);
	checkf(InWindowHandle, "FWindowsInput::Startup에 전달된 HWND가 null이다.");
	WindowHandle = InWindowHandle;

	RAWINPUTDEVICE RawMouse = {};
	RawMouse.usUsagePage = HID_USAGE_PAGE_GENERIC;
	RawMouse.usUsage = HID_USAGE_GENERIC_MOUSE;
	RawMouse.hwndTarget = WindowHandle;

	// Raw Input은 RawPointerDelta 전용이며 실패해도 WM_MOUSEMOVE 기반 입력은 계속 동작한다.
	if (!RegisterRawInputDevices(&RawMouse, 1, sizeof(RawMouse)))
	{
		const DWORD Error = GetLastError();
		KE_LOG(LogInput, Warning, "마우스 Raw Input 등록 실패. RawPointerDelta를 사용할 수 없다. GetLastError()={}", Error);
	}
}

void FWindowsInput::Shutdown()
{
	if (!WindowHandle)
	{
		return;
	}

	RAWINPUTDEVICE RawMouse = {};
	RawMouse.usUsagePage = HID_USAGE_PAGE_GENERIC;
	RawMouse.usUsage = HID_USAGE_GENERIC_MOUSE;
	RawMouse.dwFlags = RIDEV_REMOVE;
	verify(RegisterRawInputDevices(&RawMouse, 1, sizeof(RawMouse)));

	WindowHandle = nullptr;
	PendingEvents.clear();
}

// Win32 키보드, 포인터, 문자, 포커스 메시지를 프레임 입력 상태와 순서 보존 이벤트로 변환한다.
void FWindowsInput::ProcessMessage(HWND MessageWindowHandle, UINT Message, WPARAM WParam, LPARAM LParam)
{
	if (MessageWindowHandle != WindowHandle)
	{
		return;
	}

	switch (Message)
	{
	// Window Focus
	case WM_SETFOCUS:
		ProcessFocusChange(true);
		break;

	case WM_KILLFOCUS:
		ProcessFocusChange(false);
		break;

	// Keyboard
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		ProcessKeyDown(WParam, LParam);
		break;

	case WM_KEYUP:
	case WM_SYSKEYUP:
		ProcessKeyUp(WParam, LParam);
		break;

	// Character
	case WM_CHAR:
		ProcessUtf16Character(static_cast<char16_t>(WParam), static_cast<uint16>(LOWORD(LParam)));
		break;

	case WM_UNICHAR:
		ProcessUtf32Character(WParam, static_cast<uint16>(LOWORD(LParam)));
		break;

	// Pointer Movement
	case WM_MOUSEMOVE:
		ProcessMouseMove(LParam);
		break;

	case WM_INPUT:
		ProcessRawInput(LParam);
		break;

	// Left Mouse Button
	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
		ProcessMouseButtonDown(EMouseButton::Left, Message == WM_LBUTTONDBLCLK, LParam);
		break;

	case WM_LBUTTONUP:
		ProcessMouseButtonUp(EMouseButton::Left, LParam);
		break;

	// Right Mouse Button
	case WM_RBUTTONDOWN:
	case WM_RBUTTONDBLCLK:
		ProcessMouseButtonDown(EMouseButton::Right, Message == WM_RBUTTONDBLCLK, LParam);
		break;

	case WM_RBUTTONUP:
		ProcessMouseButtonUp(EMouseButton::Right, LParam);
		break;

	// Middle Mouse Button
	case WM_MBUTTONDOWN:
	case WM_MBUTTONDBLCLK:
		ProcessMouseButtonDown(EMouseButton::Middle, Message == WM_MBUTTONDBLCLK, LParam);
		break;

	case WM_MBUTTONUP:
		ProcessMouseButtonUp(EMouseButton::Middle, LParam);
		break;

	// Thumb Mouse Buttons
	case WM_XBUTTONDOWN:
	case WM_XBUTTONDBLCLK:
		ProcessMouseButtonDown(TranslateXButton(WParam), Message == WM_XBUTTONDBLCLK, LParam);
		break;

	case WM_XBUTTONUP:
		ProcessMouseButtonUp(TranslateXButton(WParam), LParam);
		break;

	// Mouse Wheel and Capture
	case WM_MOUSEWHEEL:
	case WM_MOUSEHWHEEL:
		ProcessMouseWheel(Message, WParam, LParam);
		break;

	case WM_CAPTURECHANGED:
		ProcessCaptureChanged(LParam);
		break;

	default:
		break;
	}
}

FInputSnapshot FWindowsInput::TakeSnapshot()
{
	panicf(NextFrameNumber != 0, "InputSnapshot 프레임 번호 공간을 모두 소진했다.");

	FInputSnapshot Snapshot;
	Snapshot.FrameNumber = NextFrameNumber++;
	Snapshot.KeysDown = KeysDown;
	Snapshot.KeysPressed = KeysPressed;
	Snapshot.KeysReleased = KeysReleased;
	Snapshot.MouseButtonsDown = MouseButtonsDown;
	Snapshot.MouseButtonsPressed = MouseButtonsPressed;
	Snapshot.MouseButtonsReleased = MouseButtonsReleased;
	Snapshot.PointerPosition = PointerPosition;
	Snapshot.PointerDelta = PointerDelta;
	Snapshot.RawPointerDelta = RawPointerDelta;
	Snapshot.WheelDelta = WheelDelta;
	Snapshot.Modifiers = GetModifiers();
	Snapshot.Events = std::move(PendingEvents);
	Snapshot.bHasPointerPosition = bHasPointerPosition;
	Snapshot.bHasFocus = bHasFocus;

	KeysPressed.fill(false);
	KeysReleased.fill(false);
	MouseButtonsPressed = EMouseButtonMask::None;
	MouseButtonsReleased = EMouseButtonMask::None;
	PointerDelta = FVector2::ZeroVector;
	RawPointerDelta = FVector2::ZeroVector;
	WheelDelta = FVector2::ZeroVector;
	PendingEvents.clear();
	return Snapshot;
}

// 포커스 전환을 반영하고 포커스를 잃으면 조합 대기 중인 상위 서로게이트를 버린다.
void FWindowsInput::ProcessFocusChange(bool bInHasFocus)
{
	SetWindowFocusState(bInHasFocus);
	if (!bInHasFocus)
	{
		PendingHighSurrogate = 0;
	}
}

// 키 누름 메시지를 엔진 키와 반복 여부로 변환한다.
void FWindowsInput::ProcessKeyDown(WPARAM WParam, LPARAM LParam)
{
	SetKeyState(TranslateVirtualKey(WParam, LParam), true, (LParam & PreviousKeyStateMask) != 0);
}

// 키 뗌 메시지를 엔진 키 상태에 반영한다.
void FWindowsInput::ProcessKeyUp(WPARAM WParam, LPARAM LParam)
{
	SetKeyState(TranslateVirtualKey(WParam, LParam), false);
}

// UTF-32 문자 메시지의 기능 확인 값을 제외하고 문자 이벤트를 생성한다.
void FWindowsInput::ProcessUtf32Character(WPARAM WParam, uint16 RepeatCount)
{
	if (WParam != UNICODE_NOCHAR)
	{
		AddCharacter(static_cast<char32_t>(WParam), RepeatCount);
	}
}

// 마우스 메시지의 클라이언트 좌표를 포인터 위치에 반영한다.
void FWindowsInput::ProcessMouseMove(LPARAM LParam)
{
	SetPointerPosition(FVector2(static_cast<float>(GET_X_LPARAM(LParam)), static_cast<float>(GET_Y_LPARAM(LParam))));
}

// 마우스 버튼 누름과 더블 클릭을 반영하고 포인터 캡처를 시작한다.
void FWindowsInput::ProcessMouseButtonDown(EMouseButton Button, bool bDoubleClick, LPARAM LParam)
{
	ProcessMouseMove(LParam);
	SetMouseButtonState(Button, true, bDoubleClick);
	SetCapture(WindowHandle);
}

// 마우스 버튼 뗌과 해당 시점의 포인터 위치를 반영하고 남은 버튼이 없으면 캡처를 해제한다.
void FWindowsInput::ProcessMouseButtonUp(EMouseButton Button, LPARAM LParam)
{
	ProcessMouseMove(LParam);
	SetMouseButtonState(Button, false);
	ReleaseMouseCaptureIfUnused();
}

// 화면 좌표로 전달된 휠 메시지를 클라이언트 좌표와 축별 델타로 변환한다.
void FWindowsInput::ProcessMouseWheel(UINT Message, WPARAM WParam, LPARAM LParam)
{
	POINT ClientPosition = { GET_X_LPARAM(LParam), GET_Y_LPARAM(LParam) };
	const BOOL bConvertedToClient = ScreenToClient(WindowHandle, &ClientPosition);
	verify(bConvertedToClient);
	if (bConvertedToClient)
	{
		SetPointerPosition(FVector2(static_cast<float>(ClientPosition.x), static_cast<float>(ClientPosition.y)));
	}

	const float Delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(WParam)) / static_cast<float>(WHEEL_DELTA);
	AddWheelDelta(Message == WM_MOUSEWHEEL ? FVector2(0.0f, Delta) : FVector2(-Delta, 0.0f));
}

// 다른 창으로 포인터 캡처가 넘어가면 눌린 마우스 버튼을 모두 해제한다.
void FWindowsInput::ProcessCaptureChanged(LPARAM LParam)
{
	if (reinterpret_cast<HWND>(LParam) != WindowHandle)
	{
		ReleaseMouseButtons(true);
	}
}

// Windows 가상 키와 스캔 코드를 좌우 modifier 및 NumPad Enter가 구분된 엔진 키로 변환한다.
EKeyboardKey FWindowsInput::TranslateVirtualKey(WPARAM WParam, LPARAM LParam)
{
	uint32 VirtualKey = static_cast<uint32>(WParam);
	const bool bIsExtendedKey = (LParam & ExtendedKeyMask) != 0;
	if (VirtualKey == VK_SHIFT)
	{
		const uint32 ScanCode = (static_cast<uint32>(LParam) >> 16) & 0xff;
		VirtualKey = MapVirtualKeyW(ScanCode, MAPVK_VSC_TO_VK_EX);
	}
	else if (VirtualKey == VK_CONTROL)
	{
		VirtualKey = bIsExtendedKey ? VK_RCONTROL : VK_LCONTROL;
	}
	else if (VirtualKey == VK_MENU)
	{
		VirtualKey = bIsExtendedKey ? VK_RMENU : VK_LMENU;
	}
	else if (VirtualKey == VK_RETURN && bIsExtendedKey)
	{
		return EKeyboardKey::NumPadEnter;
	}

	if (!bIsExtendedKey)
	{
		static constexpr TStaticArray<EKeyboardKey, 256> NumPadNavigationMap = []
		{
			TStaticArray<EKeyboardKey, 256> Mapping = {};
			Mapping.fill(EKeyboardKey::Unknown);
			Mapping[VK_INSERT] = EKeyboardKey::NumPad0;
			Mapping[VK_END] = EKeyboardKey::NumPad1;
			Mapping[VK_DOWN] = EKeyboardKey::NumPad2;
			Mapping[VK_NEXT] = EKeyboardKey::NumPad3;
			Mapping[VK_LEFT] = EKeyboardKey::NumPad4;
			Mapping[VK_CLEAR] = EKeyboardKey::NumPad5;
			Mapping[VK_RIGHT] = EKeyboardKey::NumPad6;
			Mapping[VK_HOME] = EKeyboardKey::NumPad7;
			Mapping[VK_UP] = EKeyboardKey::NumPad8;
			Mapping[VK_PRIOR] = EKeyboardKey::NumPad9;
			Mapping[VK_DELETE] = EKeyboardKey::NumPadDecimal;
			return Mapping;
		}();

		if (VirtualKey < NumPadNavigationMap.size())
		{
			const EKeyboardKey NumPadKey = NumPadNavigationMap[VirtualKey];
			if (NumPadKey != EKeyboardKey::Unknown)
			{
				return NumPadKey;
			}
		}
	}

	static_assert(static_cast<uint16>(EKeyboardKey::Nine) - static_cast<uint16>(EKeyboardKey::Zero) == 9);
	static_assert(static_cast<uint16>(EKeyboardKey::Z) - static_cast<uint16>(EKeyboardKey::A) == 25);
	static_assert(static_cast<uint16>(EKeyboardKey::NumPad9) - static_cast<uint16>(EKeyboardKey::NumPad0) == 9);
	static_assert(static_cast<uint16>(EKeyboardKey::F12) - static_cast<uint16>(EKeyboardKey::F1) == 11);

	static constexpr TStaticArray<EKeyboardKey, 256> VirtualKeyMap = []
	{
		TStaticArray<EKeyboardKey, 256> Mapping = {};
		Mapping.fill(EKeyboardKey::Unknown);

		const auto MapRange = [&Mapping](uint32 FirstVirtualKey, EKeyboardKey FirstKey, uint32 Count)
		{
			for (uint32 Offset = 0; Offset < Count; ++Offset)
			{
				Mapping[FirstVirtualKey + Offset] = static_cast<EKeyboardKey>(
				    static_cast<uint16>(FirstKey) + Offset);
			}
		};

		MapRange('0', EKeyboardKey::Zero, 10);
		MapRange('A', EKeyboardKey::A, 26);
		MapRange(VK_NUMPAD0, EKeyboardKey::NumPad0, 10);
		MapRange(VK_F1, EKeyboardKey::F1, 12);

		Mapping[VK_BACK] = EKeyboardKey::Backspace;
		Mapping[VK_TAB] = EKeyboardKey::Tab;
		Mapping[VK_RETURN] = EKeyboardKey::Enter;
		Mapping[VK_PAUSE] = EKeyboardKey::Pause;
		Mapping[VK_CAPITAL] = EKeyboardKey::CapsLock;
		Mapping[VK_ESCAPE] = EKeyboardKey::Escape;
		Mapping[VK_SPACE] = EKeyboardKey::Space;
		Mapping[VK_PRIOR] = EKeyboardKey::PageUp;
		Mapping[VK_NEXT] = EKeyboardKey::PageDown;
		Mapping[VK_END] = EKeyboardKey::End;
		Mapping[VK_HOME] = EKeyboardKey::Home;
		Mapping[VK_LEFT] = EKeyboardKey::Left;
		Mapping[VK_UP] = EKeyboardKey::Up;
		Mapping[VK_RIGHT] = EKeyboardKey::Right;
		Mapping[VK_DOWN] = EKeyboardKey::Down;
		Mapping[VK_SNAPSHOT] = EKeyboardKey::PrintScreen;
		Mapping[VK_INSERT] = EKeyboardKey::Insert;
		Mapping[VK_DELETE] = EKeyboardKey::Delete;
		Mapping[VK_LWIN] = EKeyboardKey::LeftWindows;
		Mapping[VK_RWIN] = EKeyboardKey::RightWindows;
		Mapping[VK_APPS] = EKeyboardKey::Applications;
		Mapping[VK_MULTIPLY] = EKeyboardKey::NumPadMultiply;
		Mapping[VK_ADD] = EKeyboardKey::NumPadAdd;
		Mapping[VK_SUBTRACT] = EKeyboardKey::NumPadSubtract;
		Mapping[VK_DECIMAL] = EKeyboardKey::NumPadDecimal;
		Mapping[VK_DIVIDE] = EKeyboardKey::NumPadDivide;
		Mapping[VK_NUMLOCK] = EKeyboardKey::NumLock;
		Mapping[VK_SCROLL] = EKeyboardKey::ScrollLock;
		Mapping[VK_LSHIFT] = EKeyboardKey::LeftShift;
		Mapping[VK_RSHIFT] = EKeyboardKey::RightShift;
		Mapping[VK_LCONTROL] = EKeyboardKey::LeftControl;
		Mapping[VK_RCONTROL] = EKeyboardKey::RightControl;
		Mapping[VK_LMENU] = EKeyboardKey::LeftAlt;
		Mapping[VK_RMENU] = EKeyboardKey::RightAlt;
		Mapping[VK_OEM_1] = EKeyboardKey::Semicolon;
		Mapping[VK_OEM_PLUS] = EKeyboardKey::Equals;
		Mapping[VK_OEM_COMMA] = EKeyboardKey::Comma;
		Mapping[VK_OEM_MINUS] = EKeyboardKey::Hyphen;
		Mapping[VK_OEM_PERIOD] = EKeyboardKey::Period;
		Mapping[VK_OEM_2] = EKeyboardKey::Slash;
		Mapping[VK_OEM_3] = EKeyboardKey::Tilde;
		Mapping[VK_OEM_4] = EKeyboardKey::LeftBracket;
		Mapping[VK_OEM_5] = EKeyboardKey::Backslash;
		Mapping[VK_OEM_6] = EKeyboardKey::RightBracket;
		Mapping[VK_OEM_7] = EKeyboardKey::Apostrophe;
		return Mapping;
	}();

	return VirtualKey < VirtualKeyMap.size() ? VirtualKeyMap[VirtualKey] : EKeyboardKey::Unknown;
}

// XButton 메시지의 상위 워드 값을 엔진 Thumb 버튼으로 변환한다.
EMouseButton FWindowsInput::TranslateXButton(WPARAM WParam)
{
	return GET_XBUTTON_WPARAM(WParam) == XBUTTON1 ? EMouseButton::Thumb1 : EMouseButton::Thumb2;
}

void FWindowsInput::SetKeyState(EKeyboardKey Key, bool bDown, bool bRepeat)
{
	const SIZE_T KeyIndex = static_cast<SIZE_T>(Key);
	if (Key == EKeyboardKey::Unknown || KeyIndex >= KeyCount)
	{
		return;
	}

	const bool bWasDown = KeysDown[KeyIndex];
	if (bWasDown == bDown && !(bDown && bRepeat))
	{
		return;
	}

	KeysDown[KeyIndex] = bDown;
	if (bDown && !bWasDown)
	{
		KeysPressed[KeyIndex] = true;
	}
	else if (!bDown && bWasDown)
	{
		KeysReleased[KeyIndex] = true;
	}

	PendingEvents.emplace_back(FKeyInputEvent{
	    Key,
	    GetModifiers(),
	    bDown,
	    bRepeat,
	});
}

void FWindowsInput::SetMouseButtonState(EMouseButton Button, bool bDown, bool bDoubleClick)
{
	const EMouseButtonMask ButtonMask = GetMouseButtonMask(Button);
	if (ButtonMask == EMouseButtonMask::None)
	{
		return;
	}

	const bool bWasDown = HasMouseButton(MouseButtonsDown, ButtonMask);
	if (bWasDown == bDown)
	{
		return;
	}

	if (bDown)
	{
		MouseButtonsDown |= ButtonMask;
		MouseButtonsPressed |= ButtonMask;
	}
	else
	{
		MouseButtonsDown &= ~ButtonMask;
		MouseButtonsReleased |= ButtonMask;
	}

	PendingEvents.emplace_back(FPointerInputEvent{
	    bDown ? EPointerInputEventType::ButtonDown : EPointerInputEventType::ButtonUp,
	    Button,
	    MouseButtonsDown,
	    GetModifiers(),
	    PointerPosition,
	    FVector2::ZeroVector,
	    FVector2::ZeroVector,
	    bDoubleClick,
	});
}

void FWindowsInput::SetPointerPosition(const FVector2& InPointerPosition)
{
	if (bHasPointerPosition && PointerPosition == InPointerPosition)
	{
		return;
	}

	FVector2 EventDelta = FVector2::ZeroVector;
	if (bHasPointerPosition)
	{
		EventDelta = InPointerPosition - PointerPosition;
		PointerDelta += EventDelta;
	}
	PointerPosition = InPointerPosition;
	bHasPointerPosition = true;

	PendingEvents.emplace_back(FPointerInputEvent{
	    EPointerInputEventType::CursorMoved,
	    EMouseButton::Invalid,
	    MouseButtonsDown,
	    GetModifiers(),
	    PointerPosition,
	    EventDelta,
	});
}

void FWindowsInput::AddRawPointerDelta(const FVector2& InRawPointerDelta)
{
	if (InRawPointerDelta.IsZero())
	{
		return;
	}

	RawPointerDelta += InRawPointerDelta;
	PendingEvents.emplace_back(FPointerInputEvent{
	    EPointerInputEventType::MouseMoved,
	    EMouseButton::Invalid,
	    MouseButtonsDown,
	    GetModifiers(),
	    PointerPosition,
	    InRawPointerDelta,
	});
}

void FWindowsInput::AddWheelDelta(const FVector2& InWheelDelta)
{
	if (InWheelDelta.IsZero())
	{
		return;
	}

	WheelDelta += InWheelDelta;
	FPointerInputEvent Event;
	Event.Type = EPointerInputEventType::Wheel;
	Event.PressedButtons = MouseButtonsDown;
	Event.Modifiers = GetModifiers();
	Event.Position = PointerPosition;
	Event.WheelDelta = InWheelDelta;
	PendingEvents.emplace_back(Event);
}

void FWindowsInput::AddCharacter(char32_t Character, uint16 RepeatCount)
{
	if (Character == U'\0' || RepeatCount == 0)
	{
		return;
	}

	const EModifierKeyMask Modifiers = GetModifiers();
	for (uint16 RepeatIndex = 0; RepeatIndex < RepeatCount; ++RepeatIndex)
	{
		PendingEvents.emplace_back(FCharacterInputEvent{ Character, Modifiers });
	}
}

void FWindowsInput::SetWindowFocusState(bool bInHasFocus)
{
	if (bHasFocus == bInHasFocus)
	{
		return;
	}

	bHasFocus = bInHasFocus;
	if (!bHasFocus)
	{
		for (SIZE_T KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
		{
			if (KeysDown[KeyIndex])
			{
				KeysDown[KeyIndex] = false;
				KeysReleased[KeyIndex] = true;
			}
		}
		ReleaseMouseButtons(false);
		PointerDelta = FVector2::ZeroVector;
		RawPointerDelta = FVector2::ZeroVector;
		bHasPointerPosition = false;
	}
	PendingEvents.emplace_back(FFocusInputEvent{ bHasFocus });
}

void FWindowsInput::ReleaseMouseButtons(bool bEmitReleaseEvents)
{
	for (SIZE_T ButtonIndex = 0; ButtonIndex < MouseButtonCount; ++ButtonIndex)
	{
		const EMouseButton Button = static_cast<EMouseButton>(ButtonIndex);
		const EMouseButtonMask ButtonMask = GetMouseButtonMask(Button);
		if (!HasMouseButton(MouseButtonsDown, ButtonMask))
		{
			continue;
		}

		MouseButtonsDown &= ~ButtonMask;
		MouseButtonsReleased |= ButtonMask;
		if (bEmitReleaseEvents)
		{
			PendingEvents.emplace_back(FPointerInputEvent{
			    EPointerInputEventType::ButtonUp,
			    Button,
			    MouseButtonsDown,
			    GetModifiers(),
			    PointerPosition,
			});
		}
	}
}

// 눌린 마우스 버튼이 없을 때 이 윈도우가 보유한 포인터 캡처를 해제한다.
void FWindowsInput::ReleaseMouseCaptureIfUnused()
{
	if (MouseButtonsDown == EMouseButtonMask::None && GetCapture() == WindowHandle)
	{
		ReleaseCapture();
	}
}

void FWindowsInput::ProcessRawInput(LPARAM LParam)
{
	RAWINPUT RawInput = {};
	UINT RawInputSize = sizeof(RawInput);
	const UINT ReadSize = GetRawInputData(reinterpret_cast<HRAWINPUT>(LParam), RID_INPUT, &RawInput, &RawInputSize, sizeof(RAWINPUTHEADER));
	if (ReadSize == static_cast<UINT>(-1) || RawInput.header.dwType != RIM_TYPEMOUSE)
	{
		return;
	}
	if ((RawInput.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) != 0)
	{
		return;
	}

	AddRawPointerDelta(FVector2(static_cast<float>(RawInput.data.mouse.lLastX), static_cast<float>(RawInput.data.mouse.lLastY)));
}

// UTF-16 코드 단위를 문자로 변환하고 Surrogate 쌍은 하나의 UTF-32 값으로 조합한다.
// Surrogate: UTF-16 인코딩 방식에서 16-bit로 표현할 수 없는 문자를 표현하기 위해 예약된 값 쌍
void FWindowsInput::ProcessUtf16Character(char16_t CodeUnit, uint16 RepeatCount)
{
	static constexpr char16_t HighSurrogateStart = 0xd800;
	static constexpr char16_t HighSurrogateEnd = 0xdbff;
	static constexpr char16_t LowSurrogateStart = 0xdc00;
	static constexpr char16_t LowSurrogateEnd = 0xdfff;

	if (CodeUnit >= HighSurrogateStart && CodeUnit <= HighSurrogateEnd)
	{
		PendingHighSurrogate = CodeUnit;
		return;
	}

	const bool bIsLowSurrogate = CodeUnit >= LowSurrogateStart && CodeUnit <= LowSurrogateEnd;
	if (!bIsLowSurrogate)
	{
		AddCharacter(static_cast<char32_t>(CodeUnit), RepeatCount);
	}
	else if (PendingHighSurrogate != 0)
	{
		const char32_t HighBits = static_cast<char32_t>(PendingHighSurrogate - HighSurrogateStart);
		const char32_t LowBits = static_cast<char32_t>(CodeUnit - LowSurrogateStart);
		AddCharacter(U'\x10000' + (HighBits << 10) + LowBits, RepeatCount);
	}

	PendingHighSurrogate = 0;
}

EModifierKeyMask FWindowsInput::GetModifiers() const
{
	EModifierKeyMask Modifiers = EModifierKeyMask::None;
	if (KeysDown[static_cast<SIZE_T>(EKeyboardKey::LeftShift)] || KeysDown[static_cast<SIZE_T>(EKeyboardKey::RightShift)])
	{
		Modifiers |= EModifierKeyMask::Shift;
	}
	if (KeysDown[static_cast<SIZE_T>(EKeyboardKey::LeftControl)] || KeysDown[static_cast<SIZE_T>(EKeyboardKey::RightControl)])
	{
		Modifiers |= EModifierKeyMask::Control;
	}
	if (KeysDown[static_cast<SIZE_T>(EKeyboardKey::LeftAlt)] || KeysDown[static_cast<SIZE_T>(EKeyboardKey::RightAlt)])
	{
		Modifiers |= EModifierKeyMask::Alt;
	}
	if (KeysDown[static_cast<SIZE_T>(EKeyboardKey::LeftWindows)] || KeysDown[static_cast<SIZE_T>(EKeyboardKey::RightWindows)])
	{
		Modifiers |= EModifierKeyMask::Super;
	}
	return Modifiers;
}
