#include "LevelEditorViewportClient.h"

#include <variant>

FInputReply FLevelEditorViewportClient::OnInputEvent(const FInputEvent& Event)
{
	const FPointerInputEvent* PointerEvent = std::get_if<FPointerInputEvent>(&Event);
	if (!PointerEvent)
	{
		return FInputReply::Unhandled();
	}

	if (PointerEvent->Type == EPointerInputEventType::ButtonDown)
	{
		return FInputReply::Handled().SetKeyboardFocus().CaptureMouse();
	}
	if (PointerEvent->Type == EPointerInputEventType::ButtonUp)
	{
		return FInputReply::Handled().ReleaseMouse();
	}
	return FInputReply::Handled();
}
