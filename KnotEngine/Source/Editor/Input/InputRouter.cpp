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

// 입력 처리 결과에 클라이언트 좌표 기준의 커서 재배치 요청을 추가한다.
FInputReply& FInputReply::WarpCursor(const FVector2& Position)
{
	bHandled = true;
	CursorWarpPosition = Position;
	return *this;
}

// 캡처와 창 포커스가 유효한 커서 재배치 요청을 한 번 반환하고 비운다.
std::optional<FVector2> FInputRouter::ConsumeCursorWarp()
{
	const std::optional<FVector2> Position = MouseCaptureOwner && PendingSnapshot.HasFocus() ? PendingCursorWarp : std::nullopt;
	PendingCursorWarp.reset();
	return Position;
}

// 새 입력 스냅샷을 보관하고 프레임별 대상 등록과 소비 상태를 초기화한다.
void FInputRouter::BeginFrame(const FInputSnapshot& InputSnapshot)
{
	checkf(!bHasPendingFrame, "이전 에디터 입력 프레임을 라우팅하기 전에 새 스냅샷이 전달되었다.");

	PendingSnapshot = InputSnapshot;
	PendingCursorWarp.reset();
	RegisteredTargets.clear();
	HandledEvents.clear();
	HoveredTarget = nullptr;
	GlobalKeyTarget = nullptr;
	bImGuiWantsMouse = false;
	bImGuiWantsKeyboard = false;
	bImGuiWantsTextInput = false;
	bHasPendingFrame = true;
}

// UI 전역 단축키가 ImGui와 Viewport보다 먼저 Key Event를 소비할 수 있도록 대상 하나를 등록한다.
void FInputRouter::RegisterGlobalKeyTarget(IInputTarget& Target)
{
	checkf(bHasPendingFrame, "FInputRouter::BeginFrame()보다 먼저 전역 Key 입력 대상을 등록할 수 없다.");
	checkf(!GlobalKeyTarget || GlobalKeyTarget == &Target, "전역 Key 입력 대상은 프레임마다 하나만 등록할 수 있다.");
	GlobalKeyTarget = &Target;
}

// 매 프레임 Input Target을 새로 등록하며, 같은 Input Target 객체는 입력 라우팅이 끝날 때까지 살아 있어야 한다.
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
	if (GlobalKeyTarget == &Target)
	{
		GlobalKeyTarget = nullptr;
	}
	if (HoveredTarget == &Target)
	{
		HoveredTarget = nullptr;
	}
	if (KeyboardFocusOwner == &Target)
	{
		KeyboardFocusOwner = nullptr;
		Target.OnKeyboardFocusLost();
	}
	if (MouseCaptureOwner == &Target)
	{
		MouseCaptureOwner = nullptr;
		Target.OnMouseCaptureLost();
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

	RegisteredTargets.erase(
		std::remove_if(RegisteredTargets.begin(), RegisteredTargets.end(), [&Target](const FRegisteredTarget& RegisteredTarget) { return RegisteredTarget.Target == &Target; }),
		RegisteredTargets.end());
}

void FInputRouter::SetImGuiCaptureState(bool bWantsMouse, bool bWantsKeyboard, bool bWantsTextInput)
{
	// NewFrame 직후와 UI 구성 이후의 상태를 모두 보존한다. 텍스트 위젯이 Enter로
	// 포커스를 해제한 프레임에도 해당 KeyDown/KeyUp이 뷰포트로 새지 않아야 한다.
	bImGuiWantsMouse |= bWantsMouse;
	bImGuiWantsKeyboard |= bWantsKeyboard;
	bImGuiWantsTextInput |= bWantsTextInput;
}

// 스냅샷 이벤트를 순서대로 전달하고 입력 소유권과 유지 입력을 정리한다.
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
	// UI가 키보드 입력을 점유하면 유지 입력을 취소한다. Viewport Tick 자체는 계속 실행한다.
	if (bImGuiWantsKeyboard || bImGuiWantsTextInput)
	{
		SetKeyboardFocus(nullptr);
	}

	bHasPendingFrame = false;
}

// 입력 소유권을 해제하고 스냅샷과 라우터 상태를 초기화한다.
void FInputRouter::Reset()
{
	ClearAllOwnership(true);
	PendingSnapshot = {};
	PendingCursorWarp.reset();
	RegisteredTargets.clear();
	HandledEvents.clear();
	HoveredTarget = nullptr;
	GlobalKeyTarget = nullptr;
	bImGuiWantsMouse = false;
	bImGuiWantsKeyboard = false;
	bImGuiWantsTextInput = false;
	bHasPendingFrame = false;
}

bool FInputRouter::HasMouseInput(const IInputTarget& Target) const
{
	return MouseCaptureOwner == &Target || HoveredTarget == &Target;
}

bool FInputRouter::HasKeyboardInput(const IInputTarget& Target) const
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

// 키 입력의 Down 소유권과 드래그 상태에 따라 단축키·UI·포커스 대상으로 전달한다.
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
	else if (!MouseCaptureOwner && !(bImGuiWantsMouse && IsAnyMouseButtonDown()) && DispatchEvent(GlobalKeyTarget, FInputEvent(Event)))
	{
		SequenceOwner = { GlobalKeyTarget, ESequenceOwner::Native };
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

// 마우스 버튼 소유권과 캡처·Hover 상태에 따라 포인터 이벤트를 전달한다.
bool FInputRouter::RoutePointerEvent(const FPointerInputEvent& Event)
{
	if (Event.Type == EPointerInputEventType::CursorMoved)
	{
		PendingCursorWarp.reset();
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

// 입력 대상이 반환한 포커스·캡처 변경과 커서 재배치 요청을 적용한다.
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
	if (Reply.CursorWarpPosition && MouseCaptureOwner == &Target)
	{
		PendingCursorWarp = Reply.CursorWarpPosition;
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

// 마우스 캡처 대상을 교체하고 이전 대상에 캡처 상실을 통지한다.
void FInputRouter::SetMouseCapture(IInputTarget* Target)
{
	if (MouseCaptureOwner == Target)
	{
		return;
	}

	IInputTarget* PreviousOwner = MouseCaptureOwner;
	PendingCursorWarp.reset();
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

// 모든 입력 소유권과 미실행 커서 요청을 비우고 필요하면 상실을 통지한다.
void FInputRouter::ClearAllOwnership(bool bNotifyOwners)
{
	PendingCursorWarp.reset();
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
	return Target && (Target == GlobalKeyTarget || std::any_of(
		RegisteredTargets.begin(),
		RegisteredTargets.end(),
		[Target](const FRegisteredTarget& RegisteredTarget)
		{
			return RegisteredTarget.Target == Target;
		}));
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
