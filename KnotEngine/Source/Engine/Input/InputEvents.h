#pragma once

#include "Core/Math/Vector2.h"
#include "Input/InputKeys.h"

#include <variant>

// 키 상태 변화와 당시 modifier key 및 반복 여부를 전달한다.
struct FKeyInputEvent
{
	EKeyboardKey Key = EKeyboardKey::Unknown;
	EModifierKeyMask Modifiers = EModifierKeyMask::None;
	bool bDown = false;
	bool bRepeat = false;
};

// 포인터 입력 이벤트의 이동, 버튼 및 휠 종류를 구분한다.
enum class EPointerInputEventType : uint8
{
	Moved,
	RawMoved,
	ButtonDown,
	ButtonUp,
	Wheel,
};

// 포인터 위치와 이동량, 버튼과 더블 클릭 여부 및 휠 상태를 전달한다.
struct FPointerInputEvent
{
	EPointerInputEventType Type = EPointerInputEventType::Moved;
	EMouseButton Button = EMouseButton::Invalid;
	EMouseButtonMask PressedButtons = EMouseButtonMask::None;
	EModifierKeyMask Modifiers = EModifierKeyMask::None;
	FVector2 Position = FVector2::ZeroVector;
	FVector2 Delta = FVector2::ZeroVector;
	FVector2 WheelDelta = FVector2::ZeroVector;
	bool bDoubleClick = false;
};

// 텍스트 입력으로 생성된 UTF-32 문자와 modifier key 상태를 전달한다.
struct FCharacterInputEvent
{
    // 향후 다중 윈도우가 추가될 경우 확장 가능하다.
	char32_t Character = U'\0';
	EModifierKeyMask Modifiers = EModifierKeyMask::None;
};

// 네이티브 윈도우의 입력 포커스 획득과 상실을 전달한다.
struct FFocusInputEvent
{
	// 향후 다중 윈도우가 추가될 경우 확장 가능하다.
	bool bHasFocus = false;
};

// 모든 입력 이벤트 구조체 중 하나를 타입 안전하게 보관한다.
using FInputEvent = std::variant<std::monostate, FKeyInputEvent, FPointerInputEvent, FCharacterInputEvent, FFocusInputEvent>;
