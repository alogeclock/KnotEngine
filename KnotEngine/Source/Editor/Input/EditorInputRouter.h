#pragma once

#include "Input/InputSnapshot.h"

// 에디터 입력 대상이 이벤트 처리 결과와 함께 포커스 및 캡처 변경을 요청한다.
class FEditorInputReply
{
public:
	static FEditorInputReply Handled();
	static FEditorInputReply Unhandled();

	FEditorInputReply& SetKeyboardFocus();
	FEditorInputReply& ClearKeyboardFocus();
	FEditorInputReply& CaptureMouse();
	FEditorInputReply& ReleaseMouse();

	bool IsHandled() const { return bHandled; }

private:
	friend class FEditorInputRouter;

	bool bHandled = false;
	bool bSetKeyboardFocus = false;
	bool bClearKeyboardFocus = false;
	bool bCaptureMouse = false;
	bool bReleaseMouse = false;
};

// ImGui 바깥에서 동작하는 뷰포트, 기즈모 등의 에디터 입력 소비 지점이다.
class IEditorInputTarget
{
public:
	virtual ~IEditorInputTarget() = default;

	virtual FEditorInputReply OnInputEvent(const FInputEvent& Event) = 0;
	virtual void OnKeyboardFocusLost() {}
	virtual void OnMouseCaptureLost() {}
};

// ImGui의 UI 판정과 엔진 입력 이벤트 사이에서 에디터 대상의 포커스 및 캡처를 관리한다.
// 대상 등록은 프레임 단위이며, 같은 대상 객체는 입력 라우팅이 끝날 때까지 살아 있어야 한다.
class FEditorInputRouter final
{
public:
	void BeginFrame(const FInputSnapshot& InputSnapshot);
	void RegisterTarget(IEditorInputTarget& Target, bool bHovered, bool bFocused);
	void UnregisterTarget(IEditorInputTarget& Target);

	void SetImGuiCaptureState(bool bWantsMouse, bool bWantsKeyboard, bool bWantsTextInput);
	void RouteInput();
	void Reset();

	IEditorInputTarget* GetHoveredTarget() const { return HoveredTarget; }
	IEditorInputTarget* GetKeyboardFocusOwner() const { return KeyboardFocusOwner; }
	IEditorInputTarget* GetMouseCaptureOwner() const { return MouseCaptureOwner; }

	bool DoesTargetOwnMouseInput(const IEditorInputTarget& Target) const;
	bool DoesTargetOwnKeyboardInput(const IEditorInputTarget& Target) const;

	const TArray<bool>& GetHandledEvents() const { return HandledEvents; }

private:
	enum class ESequenceOwner : uint8
	{
		None,
		EditorTarget,
		ImGui,
	};

	struct FSequenceOwner
	{
		IEditorInputTarget* Target = nullptr;
		ESequenceOwner Owner = ESequenceOwner::None;
	};

	struct FRegisteredTarget
	{
		IEditorInputTarget* Target = nullptr;
		bool bHovered = false;
		bool bFocused = false;
	};

	static constexpr SIZE_T KeyCount = static_cast<SIZE_T>(EKeyboardKey::Count);
	static constexpr SIZE_T MouseButtonCount = static_cast<SIZE_T>(EMouseButton::Count);

	bool RouteEvent(const FInputEvent& Event);
	bool RouteKeyEvent(const FKeyInputEvent& Event);
	bool RoutePointerEvent(const FPointerInputEvent& Event);
	bool RouteCharacterEvent(const FCharacterInputEvent& Event);
	bool RouteFocusEvent(const FFocusInputEvent& Event);

	bool DispatchEvent(IEditorInputTarget* Target, const FInputEvent& Event);
	void ApplyReply(IEditorInputTarget& Target, const FEditorInputReply& Reply);
	void ResolveFrameTargets();
	void ValidatePersistentOwners();
	void SetKeyboardFocus(IEditorInputTarget* Target);
	void SetMouseCapture(IEditorInputTarget* Target);
	void ClearSequenceOwners();
	void ClearAllOwnership(bool bNotifyOwners);

	bool IsTargetRegistered(const IEditorInputTarget* Target) const;
	bool IsAnyMouseButtonDown() const;

	FInputSnapshot PendingSnapshot;
	TArray<FRegisteredTarget> RegisteredTargets;
	TArray<bool> HandledEvents;
	TStaticArray<FSequenceOwner, KeyCount> KeyOwners = {};
	TStaticArray<FSequenceOwner, MouseButtonCount> MouseButtonOwners = {};

	IEditorInputTarget* HoveredTarget = nullptr;
	IEditorInputTarget* KeyboardFocusOwner = nullptr;
	IEditorInputTarget* MouseCaptureOwner = nullptr;

	bool bImGuiWantsMouse = false;
	bool bImGuiWantsKeyboard = false;
	bool bImGuiWantsTextInput = false;
	bool bHasPendingFrame = false;
};
