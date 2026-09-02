#include "Input/InputRouter.h"

#include "Core/Assert.h"

#include <algorithm>
#include <type_traits>

FInputReply FInputReply::Handled()
{
	FInputReply Reply;
	Reply.bHandled = true;
	return Reply;
}

FInputReply FInputReply::Unhandled()
{
	return {};
}

FInputReply& FInputReply::SetKeyboardFocus()
{
	bHandled = true;
	bSetKeyboardFocus = true;
	bClearKeyboardFocus = false;
	return *this;
}

FInputReply& FInputReply::ClearKeyboardFocus()
{
	bHandled = true;
	bClearKeyboardFocus = true;
	bSetKeyboardFocus = false;
	return *this;
}

FInputReply& FInputReply::CaptureMouse()
{
	bHandled = true;
	bCaptureMouse = true;
	bReleaseMouse = false;
	return *this;
}

FInputReply& FInputReply::ReleaseMouse()
{
	bHandled = true;
	bReleaseMouse = true;
	bCaptureMouse = false;
	return *this;
}

void FInputRouter::BeginFrame(const FInputSnapshot& InputSnapshot)
{
	checkf(!bHasPendingFrame, "이전 에디터 입력 프레임을 라우팅하기 전에 새 스냅샷이 전달되었다.");

	PendingSnapshot = InputSnapshot;
	RegisteredTargets.clear();
	HandledEvents.clear();
	HoveredTarget = nullptr;
	bImGuiWantsMouse = false;
	bImGuiWantsKeyboard = false;
	bImGuiWantsTextInput = false;
	bHasPendingFrame = true;
}

void FInputRouter::RegisterTarget(IInputTarget& Target, bool bHovered, bool bFocused)
{
	checkf(bHasPendingFrame, "FInputRouter::BeginFrame()보다 먼저 입력 대상을 등록할 수 없다.");

	for (FRegisteredTarget& RegisteredTarget : RegisteredTargets)
	{
		if (RegisteredTarget.Target == &Target)
		{
			RegisteredTarget.bHovered = bHovered;
			RegisteredTarget.bFocused = bFocused;
			return;
		}
	}

	RegisteredTargets.push_back({ &Target, bHovered, bFocused });
}

void FInputRouter::UnregisterTarget(IInputTarget& Target)
{
	RegisteredTargets.erase(
		std::remove_if(
			RegisteredTargets.begin(),
			RegisteredTargets.end(),
			[&Target](const FRegisteredTarget& RegisteredTarget)
			{
				return RegisteredTarget.Target == &Target;
			}),
		RegisteredTargets.end());

	if (HoveredTarget == &Target)
	{
		HoveredTarget = nullptr;
	}
	if (KeyboardFocusOwner == &Target)
	{
		SetKeyboardFocus(nullptr);
	}
	if (MouseCaptureOwner == &Target)
	{
		SetMouseCapture(nullptr);
	}

	for (FSequenceOwner& Owner : KeyOwners)
	{
		if (Owner.Target == &Target)
		{
			Owner = {};
		}
	}
	for (FSequenceOwner& Owner : MouseButtonOwners)
	{
		if (Owner.Target == &Target)
		{
			Owner = {};
		}
	}
}

void FInputRouter::SetImGuiCaptureState(bool bWantsMouse, bool bWantsKeyboard, bool bWantsTextInput)
{
	// NewFrame 직후와 UI 구성 이후의 상태를 모두 보존한다. 텍스트 위젯이 Enter로
	// 포커스를 해제한 프레임에도 해당 KeyDown/KeyUp이 뷰포트로 새지 않아야 한다.
	bImGuiWantsMouse |= bWantsMouse;
	bImGuiWantsKeyboard |= bWantsKeyboard;
	bImGuiWantsTextInput |= bWantsTextInput;
}

void FInputRouter::RouteInput()
{
	checkf(bHasPendingFrame, "라우팅할 에디터 입력 프레임이 없다.");

	ResolveFrameTargets();
	ValidatePersistentOwners();

	const TArray<FInputEvent>& Events = PendingSnapshot.GetEvents();
	HandledEvents.reserve(Events.size());
	for (const FInputEvent& Event : Events)
	{
		HandledEvents.push_back(RouteEvent(Event));
	}

	if (!PendingSnapshot.HasFocus())
	{
		ClearAllOwnership(true);
	}
	else if (!IsAnyMouseButtonDown())
	{
		SetMouseCapture(nullptr);
	}

	bHasPendingFrame = false;
}

void FInputRouter::Reset()
{
	ClearAllOwnership(true);
	PendingSnapshot = {};
	RegisteredTargets.clear();
	HandledEvents.clear();
	HoveredTarget = nullptr;
	bImGuiWantsMouse = false;
	bImGuiWantsKeyboard = false;
	bImGuiWantsTextInput = false;
	bHasPendingFrame = false;
}

bool FInputRouter::DoesTargetOwnMouseInput(const IInputTarget& Target) const
{
	return MouseCaptureOwner == &Target || HoveredTarget == &Target;
}

bool FInputRouter::DoesTargetOwnKeyboardInput(const IInputTarget& Target) const
{
	return KeyboardFocusOwner == &Target && !bImGuiWantsKeyboard && !bImGuiWantsTextInput;
}

bool FInputRouter::RouteEvent(const FInputEvent& Event)
{
	return std::visit(
		[this](const auto& TypedEvent)
		{
			// remove_cvref_t는 TypedEvent의 참조와 const/volatile 한정자를 제거해 실제 이벤트 타입을 얻는다.
			using EventType = std::remove_cvref_t<decltype(TypedEvent)>;

			if constexpr (std::is_same_v<EventType, FKeyInputEvent>)
			{
				return RouteKeyEvent(TypedEvent);
			}
			else if constexpr (std::is_same_v<EventType, FPointerInputEvent>)
			{
				return RoutePointerEvent(TypedEvent);
			}
			else if constexpr (std::is_same_v<EventType, FCharacterInputEvent>)
			{
				return RouteCharacterEvent(TypedEvent);
			}
			else if constexpr (std::is_same_v<EventType, FFocusInputEvent>)
			{
				return RouteFocusEvent(TypedEvent);
			}
			else
			{
				return false;
			}
		},
		Event);
}

bool FInputRouter::RouteKeyEvent(const FKeyInputEvent& Event)
{
	const SIZE_T KeyIndex = static_cast<SIZE_T>(Event.Key);
	if (Event.Key == EKeyboardKey::Unknown || KeyIndex >= KeyOwners.size())
	{
		return false;
	}

	FSequenceOwner& SequenceOwner = KeyOwners[KeyIndex];
	if (!Event.bDown)
	{
		const FSequenceOwner Owner = SequenceOwner;
		SequenceOwner = {};
		if (Owner.Owner == ESequenceOwner::ImGui)
		{
			return true;
		}
		if (Owner.Owner == ESequenceOwner::Native)
		{
			DispatchEvent(Owner.Target, FInputEvent(Event));
			return true;
		}
	}
	else if (Event.bRepeat && SequenceOwner.Owner != ESequenceOwner::None)
	{
		if (SequenceOwner.Owner == ESequenceOwner::ImGui)
		{
			return true;
		}
		DispatchEvent(SequenceOwner.Target, FInputEvent(Event));
		return true;
	}

	if (bImGuiWantsKeyboard || bImGuiWantsTextInput)
	{
		if (Event.bDown)
		{
			SequenceOwner = { nullptr, ESequenceOwner::ImGui };
		}
		return true;
	}

	const bool bHandled = DispatchEvent(KeyboardFocusOwner, FInputEvent(Event));
	if (Event.bDown && bHandled)
	{
		SequenceOwner = { KeyboardFocusOwner, ESequenceOwner::Native };
	}
	return bHandled;
}

bool FInputRouter::RoutePointerEvent(const FPointerInputEvent& Event)
{
	if (Event.Type == EPointerInputEventType::MouseMoved)
	{
		if (MouseCaptureOwner)
		{
			return DispatchEvent(MouseCaptureOwner, FInputEvent(Event));
		}
		return bImGuiWantsMouse;
	}

	IInputTarget* Target = MouseCaptureOwner ? MouseCaptureOwner : HoveredTarget;
	if (Event.Type == EPointerInputEventType::ButtonDown || Event.Type == EPointerInputEventType::ButtonUp)
	{
		const SIZE_T ButtonIndex = static_cast<SIZE_T>(Event.Button);
		if (Event.Button == EMouseButton::Invalid || ButtonIndex >= MouseButtonOwners.size())
		{
			return false;
		}

		FSequenceOwner& SequenceOwner = MouseButtonOwners[ButtonIndex];
		if (Event.Type == EPointerInputEventType::ButtonUp)
		{
			const FSequenceOwner Owner = SequenceOwner;
			SequenceOwner = {};
			if (Owner.Owner == ESequenceOwner::ImGui)
			{
				return true;
			}
			if (Owner.Owner == ESequenceOwner::Native)
			{
				DispatchEvent(Owner.Target, FInputEvent(Event));
				return true;
			}
		}

		if (!Target && bImGuiWantsMouse)
		{
			if (Event.Type == EPointerInputEventType::ButtonDown)
			{
				SequenceOwner = { nullptr, ESequenceOwner::ImGui };
				SetKeyboardFocus(nullptr);
			}
			return true;
		}

		const bool bHandled = DispatchEvent(Target, FInputEvent(Event));
		if (Event.Type == EPointerInputEventType::ButtonDown && bHandled)
		{
			SequenceOwner = { Target, ESequenceOwner::Native };
		}
		else if (Event.Type == EPointerInputEventType::ButtonDown && bImGuiWantsMouse)
		{
			SequenceOwner = { nullptr, ESequenceOwner::ImGui };
		}
		return bHandled || bImGuiWantsMouse;
	}

	if (Target)
	{
		return DispatchEvent(Target, FInputEvent(Event)) || bImGuiWantsMouse;
	}
	return bImGuiWantsMouse;
}

bool FInputRouter::RouteCharacterEvent(const FCharacterInputEvent& Event)
{
	if (bImGuiWantsTextInput || bImGuiWantsKeyboard)
	{
		return true;
	}
	return DispatchEvent(KeyboardFocusOwner, FInputEvent(Event));
}

bool FInputRouter::RouteFocusEvent(const FFocusInputEvent& Event)
{
	if (!Event.bHasFocus)
	{
		ClearAllOwnership(true);
	}
	return false;
}

bool FInputRouter::DispatchEvent(IInputTarget* Target, const FInputEvent& Event)
{
	if (!Target || !IsTargetRegistered(Target))
	{
		return false;
	}

	const FInputReply Reply = Target->OnInputEvent(Event);
	ApplyReply(*Target, Reply);
	return Reply.IsHandled();
}

void FInputRouter::ApplyReply(IInputTarget& Target, const FInputReply& Reply)
{
	if (Reply.bClearKeyboardFocus)
	{
		SetKeyboardFocus(nullptr);
	}
	if (Reply.bReleaseMouse && MouseCaptureOwner == &Target)
	{
		SetMouseCapture(nullptr);
	}
	if (Reply.bSetKeyboardFocus)
	{
		SetKeyboardFocus(&Target);
	}
	if (Reply.bCaptureMouse)
	{
		SetMouseCapture(&Target);
	}
}

void FInputRouter::ResolveFrameTargets()
{
	bool bResolvedFocusedTarget = false;
	for (auto It = RegisteredTargets.rbegin(); It != RegisteredTargets.rend(); ++It)
	{
		if (!HoveredTarget && It->bHovered)
		{
			HoveredTarget = It->Target;
		}
		if (!bResolvedFocusedTarget && It->bFocused)
		{
			SetKeyboardFocus(It->Target);
			bResolvedFocusedTarget = true;
		}
	}
}

// 이전 프레임 소유자가 현재 프레임에서도 등록되었는지 확인하고, 그렇지 않다면 정리한다.
void FInputRouter::ValidatePersistentOwners()
{
	if (KeyboardFocusOwner && !IsTargetRegistered(KeyboardFocusOwner))
	{
		KeyboardFocusOwner = nullptr;
	}
	if (MouseCaptureOwner && !IsTargetRegistered(MouseCaptureOwner))
	{
		MouseCaptureOwner = nullptr;
	}

	for (FSequenceOwner& Owner : KeyOwners)
	{
		if (Owner.Owner == ESequenceOwner::Native && !IsTargetRegistered(Owner.Target))
		{
			Owner = {};
		}
	}
	for (FSequenceOwner& Owner : MouseButtonOwners)
	{
		if (Owner.Owner == ESequenceOwner::Native && !IsTargetRegistered(Owner.Target))
		{
			Owner = {};
		}
	}
}

void FInputRouter::SetKeyboardFocus(IInputTarget* Target)
{
	if (KeyboardFocusOwner == Target)
	{
		return;
	}

	IInputTarget* PreviousOwner = KeyboardFocusOwner;
	KeyboardFocusOwner = Target;
	if (PreviousOwner && IsTargetRegistered(PreviousOwner))
	{
		PreviousOwner->OnKeyboardFocusLost();
	}
}

void FInputRouter::SetMouseCapture(IInputTarget* Target)
{
	if (MouseCaptureOwner == Target)
	{
		return;
	}

	IInputTarget* PreviousOwner = MouseCaptureOwner;
	MouseCaptureOwner = Target;
	if (PreviousOwner && IsTargetRegistered(PreviousOwner))
	{
		PreviousOwner->OnMouseCaptureLost();
	}
}

void FInputRouter::ClearSequenceOwners()
{
	KeyOwners.fill(FSequenceOwner{});
	MouseButtonOwners.fill(FSequenceOwner{});
}

void FInputRouter::ClearAllOwnership(bool bNotifyOwners)
{
	IInputTarget* PreviousKeyboardOwner = KeyboardFocusOwner;
	IInputTarget* PreviousMouseOwner = MouseCaptureOwner;

	KeyboardFocusOwner = nullptr;
	MouseCaptureOwner = nullptr;
	HoveredTarget = nullptr;
	ClearSequenceOwners();

	if (!bNotifyOwners)
	{
		return;
	}
	if (PreviousKeyboardOwner && IsTargetRegistered(PreviousKeyboardOwner))
	{
		PreviousKeyboardOwner->OnKeyboardFocusLost();
	}
	if (PreviousMouseOwner && IsTargetRegistered(PreviousMouseOwner))
	{
		PreviousMouseOwner->OnMouseCaptureLost();
	}
}

bool FInputRouter::IsTargetRegistered(const IInputTarget* Target) const
{
	return Target && std::any_of(
		RegisteredTargets.begin(),
		RegisteredTargets.end(),
		[Target](const FRegisteredTarget& RegisteredTarget)
		{
			return RegisteredTarget.Target == Target;
		});
}

bool FInputRouter::IsAnyMouseButtonDown() const
{
	for (SIZE_T ButtonIndex = 0; ButtonIndex < MouseButtonCount; ++ButtonIndex)
	{
		if (PendingSnapshot.IsMouseButtonDown(static_cast<EMouseButton>(ButtonIndex)))
		{
			return true;
		}
	}
	return false;
}
