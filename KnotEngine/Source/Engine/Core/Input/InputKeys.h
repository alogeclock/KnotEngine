#pragma once

#include "Core/CoreTypes.h"

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

constexpr EMouseButtonMask operator&(EMouseButtonMask Left, EMouseButtonMask Right) noexcept
{
	return static_cast<EMouseButtonMask>(static_cast<uint8>(Left) & static_cast<uint8>(Right));
}

constexpr EMouseButtonMask operator~(EMouseButtonMask Value) noexcept
{
	return static_cast<EMouseButtonMask>(static_cast<uint8>(~static_cast<uint8>(Value)));
}

constexpr EMouseButtonMask& operator&=(EMouseButtonMask& Left, EMouseButtonMask Right) noexcept
{
	Left = Left & Right;
	return Left;
}

constexpr EMouseButtonMask GetMouseButtonMask(EMouseButton Button) noexcept
{
	static_assert(static_cast<uint8>(EMouseButton::Count) <= 8);

	const uint8 ButtonIndex = static_cast<uint8>(Button);
	return ButtonIndex < static_cast<uint8>(EMouseButton::Count)
	           ? static_cast<EMouseButtonMask>(1u << ButtonIndex)
	           : EMouseButtonMask::None;
}

constexpr bool HasMouseButton(EMouseButtonMask Value, EMouseButtonMask Button) noexcept
{
	return (static_cast<uint8>(Value) & static_cast<uint8>(Button)) != 0;
}

// 입력 이벤트 발생 시 함께 눌린 modifier key를 표현하는 비트 마스크를 정의한다.
enum class EModifierKeyMask : uint8
{
	None = 0,
	Shift = 1 << 0,
	Control = 1 << 1,
	Alt = 1 << 2,
	Super = 1 << 3,
};

constexpr EModifierKeyMask operator|(EModifierKeyMask Left, EModifierKeyMask Right) noexcept
{
	return static_cast<EModifierKeyMask>(static_cast<uint8>(Left) | static_cast<uint8>(Right));
}

constexpr EModifierKeyMask& operator|=(EModifierKeyMask& Left, EModifierKeyMask Right) noexcept
{
	Left = Left | Right;
	return Left;
}

constexpr EModifierKeyMask operator&(EModifierKeyMask Left, EModifierKeyMask Right) noexcept
{
	return static_cast<EModifierKeyMask>(static_cast<uint8>(Left) & static_cast<uint8>(Right));
}

constexpr EModifierKeyMask operator~(EModifierKeyMask Value) noexcept
{
	return static_cast<EModifierKeyMask>(static_cast<uint8>(~static_cast<uint8>(Value)));
}

constexpr EModifierKeyMask& operator&=(EModifierKeyMask& Left, EModifierKeyMask Right) noexcept
{
	Left = Left & Right;
	return Left;
}

constexpr bool HasModifierKey(EModifierKeyMask Value, EModifierKeyMask Modifier) noexcept
{
	return (static_cast<uint8>(Value) & static_cast<uint8>(Modifier)) != 0;
}
