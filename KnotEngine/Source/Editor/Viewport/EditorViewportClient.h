#pragma once

#include "Core/CoreTypes.h"
#include "Core/Math/Rotator.h"
#include "Core/Math/Vector.h"
#include "Input/InputRouter.h"
#include "EditorViewportCamera.h"
#include "Render/Scene/SceneView.h"
#include <optional>

struct FMatrix;
class FViewport;
class FRenderer;
class UWorld;

// 메인 스레드에서 카메라와 입력을 관리하고 ViewFamily를 구성한다.
class FEditorViewportClient : public IInputTarget
{
public:
	static constexpr float CameraMoveSpeed = 500.0f;
	static constexpr float DefaultCameraSensitivity = 1.0f;
	static constexpr float MinCameraSensitivity = 0.1f;
	static constexpr float MaxCameraSensitivity = 20.0f;

	explicit FEditorViewportClient(FViewport& InViewport);
	virtual ~FEditorViewportClient();

	virtual void Tick(float DeltaTime);

	virtual UWorld* GetWorld() const = 0;
	FScene* GetScene() const;

	// Render State
	virtual FSceneView BuildSceneView();
	virtual std::optional<FSceneViewFamily> BuildSceneViewFamily();
	FShowFlags& GetShowFlags() { return ShowFlags; }
	EViewMode& GetViewMode() { return ViewMode; }
	void SetInputRect(const FVector2& Position, const FVector2& Size);

	// Input
	FInputReply OnInputEvent(const FInputEvent& Event) override;
	void OnKeyboardFocusLost() override;
	void OnMouseCaptureLost() override;
	bool ShouldHideCursor() const override { return bRotatingCamera; }

	// Camera
	FEditorViewportCamera& GetCamera() { return Camera; }
	const FEditorViewportCamera& GetCamera() const { return Camera; }
	void OnCameraStateChanged();
	void OnViewTransformChanged();

	bool IsCameraDragging() const { return bRotatingCamera; }

protected:
	FViewport& GetViewport() const { return Viewport; }
	bool GetViewportPixelPosition(const FVector2& InputPosition, FVector2& OutPixelPosition) const;
	bool ContainsInputPosition(const FVector2& InputPosition) const;

private:
	bool UpdateKeyState(const FKeyInputEvent& Event);
	bool IsKeyDown(EKeyboardKey Key) const;

	FViewport& Viewport;
	FShowFlags ShowFlags;
	EViewMode ViewMode = EViewMode::Unlit;

	TBitset<static_cast<SIZE_T>(EKeyboardKey::Count)> KeysDown; // 전역 Key 상태를 복제하는 것이 아닌, 유지 중인 키를 기억하는 상태

	FEditorViewportCamera Camera;
	FVector2 InputRectPosition = FVector2::ZeroVector;
	FVector2 InputRectSize = FVector2::ZeroVector;
	bool bRotatingCamera = false;
};
