#pragma once

#include "Core/CoreTypes.h"
#include "Core/Math/Vector2.h"

#include <variant>

// 엔진에서 식별할 수 있는 플랫폼 독립 키보드 키를 정의한다.
enum class EKeyboardKey : uint16
{
	Unknown,
	Backspace,
	Tab,
	Enter,
	Pause,
	CapsLock,
	Escape,
	Space,
	PageUp,
	PageDown,
	End,
	Home,
	Left,
	Up,
	Right,
	Down,
	PrintScreen,
	Insert,
	Delete,
	Zero,
	One,
	Two,
	Three,
	Four,
	Five,
	Six,
	Seven,
	Eight,
	Nine,
	A,
	B,
	C,
	D,
	E,
	F,
	G,
	H,
	I,
	J,
	K,
	L,
	M,
	N,
	O,
	P,
	Q,
	R,
	S,
	T,
	U,
	V,
	W,
	X,
	Y,
	Z,
	LeftWindows,
	RightWindows,
	Applications,
	NumPad0,
	NumPad1,
	NumPad2,
	NumPad3,
	NumPad4,
	NumPad5,
	NumPad6,
	NumPad7,
	NumPad8,
	NumPad9,
	NumPadMultiply,
	NumPadAdd,
	NumPadSubtract,
	NumPadDecimal,
	NumPadDivide,
	NumPadEnter,
	F1,
	F2,
	F3,
	F4,
	F5,
	F6,
	F7,
	F8,
	F9,
	F10,
	F11,
	F12,
	NumLock,
	ScrollLock,
	LeftShift,
	RightShift,
	LeftControl,
	RightControl,
	LeftAlt,
	RightAlt,
	Semicolon,
	Equals,
	Comma,
	Hyphen,
	Period,
	Slash,
	Tilde,
	LeftBracket,
	Backslash,
	RightBracket,
	Apostrophe,
	Count,
};

// 엔진에서 식별할 수 있는 마우스 버튼을 정의한다.
enum class EMouseButton : uint8
{
	Left,
	Right,
	Middle,
	Thumb1,
	Thumb2,
	Count,
	Invalid = 0xff,
};

// 동시에 눌린 마우스 버튼을 표현하는 비트 마스크를 정의한다.
enum class EMouseButtonMask : uint8
{
	None = 0,
	Left = 1 << static_cast<uint8>(EMouseButton::Left),
	Right = 1 << static_cast<uint8>(EMouseButton::Right),
	Middle = 1 << static_cast<uint8>(EMouseButton::Middle),
	Thumb1 = 1 << static_cast<uint8>(EMouseButton::Thumb1),
	Thumb2 = 1 << static_cast<uint8>(EMouseButton::Thumb2),
};

constexpr EMouseButtonMask operator|(EMouseButtonMask Left, EMouseButtonMask Right) noexcept
{
	return static_cast<EMouseButtonMask>(static_cast<uint8>(Left) | static_cast<uint8>(Right));
}

constexpr EMouseButtonMask& operator|=(EMouseButtonMask& Left, EMouseButtonMask Right) noexcept
{
	Left = Left | Right;
	return Left;
}

constexpr bool HasMouseButton(EMouseButtonMask Value, EMouseButtonMask Button) noexcept
{
	return (static_cast<uint8>(Value) & static_cast<uint8>(Button)) != 0;
}

// 입력 이벤트 발생 시 함께 눌린 modifier key를 표현하는 비트 마스크를 정의한다.
enum class EInputModifier : uint8
{
	None = 0,
	Shift = 1 << 0,
	Control = 1 << 1,
	Alt = 1 << 2,
	Super = 1 << 3,
};

constexpr EInputModifier operator|(EInputModifier Left, EInputModifier Right) noexcept
{
	return static_cast<EInputModifier>(static_cast<uint8>(Left) | static_cast<uint8>(Right));
}

constexpr EInputModifier& operator|=(EInputModifier& Left, EInputModifier Right) noexcept
{
	Left = Left | Right;
	return Left;
}

constexpr bool HasInputModifier(EInputModifier Value, EInputModifier Modifier) noexcept
{
	return (static_cast<uint8>(Value) & static_cast<uint8>(Modifier)) != 0;
}

// 키 입력 이벤트의 누름과 뗌 상태를 구분한다.
enum class EKeyInputEventType : uint8
{
	Down,
	Up,
};

// 키 상태 변화와 당시 modifier key 및 반복 여부를 전달한다.
struct FKeyInputEvent
{
	EKeyInputEventType Type = EKeyInputEventType::Down;
	EKeyboardKey Key = EKeyboardKey::Unknown;
	EInputModifier Modifiers = EInputModifier::None;
	bool bRepeat = false;
};

// 포인터 입력 이벤트의 이동, 버튼, 더블 클릭, 휠 종류를 구분한다.
enum class EPointerInputEventType : uint8
{
	Moved,
	RawMoved,
	ButtonDown,
	ButtonUp,
	DoubleClick,
	Wheel,
};

// 포인터 위치와 이동량, 버튼 및 휠 상태를 전달한다.
struct FPointerInputEvent
{
	EPointerInputEventType Type = EPointerInputEventType::Moved;
	EMouseButton Button = EMouseButton::Invalid;
	EMouseButtonMask PressedButtons = EMouseButtonMask::None;
	EInputModifier Modifiers = EInputModifier::None;
	FVector2 Position = FVector2::ZeroVector;
	FVector2 Delta = FVector2::ZeroVector;
	FVector2 WheelDelta = FVector2::ZeroVector;
};

// 텍스트 입력으로 생성된 UTF-32 문자와 modifier key 상태를 전달한다.
struct FCharacterInputEvent
{
	char32_t Character = U'\0';
	EInputModifier Modifiers = EInputModifier::None;
};

// 네이티브 윈도우의 입력 포커스 획득과 상실을 전달한다.
struct FFocusInputEvent
{
	bool bHasFocus = false;
};

// 모든 입력 이벤트 구조체 중 하나를 타입 안전하게 보관한다.
using FInputEvent = std::variant<std::monostate, FKeyInputEvent, FPointerInputEvent, FCharacterInputEvent, FFocusInputEvent>;
