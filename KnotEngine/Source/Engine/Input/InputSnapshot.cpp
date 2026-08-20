#include "Input/InputSnapshot.h"

bool FInputSnapshot::IsKeyDown(EKeyboardKey Key) const
{
	const SIZE_T KeyIndex = static_cast<SIZE_T>(Key);
	return KeyIndex < KeyCount && KeysDown[KeyIndex];
}

bool FInputSnapshot::WasKeyPressed(EKeyboardKey Key) const
{
	const SIZE_T KeyIndex = static_cast<SIZE_T>(Key);
	return KeyIndex < KeyCount && KeysPressed[KeyIndex];
}

bool FInputSnapshot::WasKeyReleased(EKeyboardKey Key) const
{
	const SIZE_T KeyIndex = static_cast<SIZE_T>(Key);
	return KeyIndex < KeyCount && KeysReleased[KeyIndex];
}

bool FInputSnapshot::IsMouseButtonDown(EMouseButton Button) const
{
	return HasMouseButton(MouseButtonsDown, GetMouseButtonMask(Button));
}

bool FInputSnapshot::WasMouseButtonPressed(EMouseButton Button) const
{
	return HasMouseButton(MouseButtonsPressed, GetMouseButtonMask(Button));
}

bool FInputSnapshot::WasMouseButtonReleased(EMouseButton Button) const
{
	return HasMouseButton(MouseButtonsReleased, GetMouseButtonMask(Button));
}
