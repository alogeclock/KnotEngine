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
	const SIZE_T ButtonIndex = static_cast<SIZE_T>(Button);
	return ButtonIndex < MouseButtonCount && MouseButtonsDown[ButtonIndex];
}

bool FInputSnapshot::WasMouseButtonPressed(EMouseButton Button) const
{
	const SIZE_T ButtonIndex = static_cast<SIZE_T>(Button);
	return ButtonIndex < MouseButtonCount && MouseButtonsPressed[ButtonIndex];
}

bool FInputSnapshot::WasMouseButtonReleased(EMouseButton Button) const
{
	const SIZE_T ButtonIndex = static_cast<SIZE_T>(Button);
	return ButtonIndex < MouseButtonCount && MouseButtonsReleased[ButtonIndex];
}
