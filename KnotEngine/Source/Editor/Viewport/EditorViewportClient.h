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
class URenderer;
class UWorld;

// 메인 스레드에서 카메라와 입력을 관리하고 ViewFamily를 구성한다.
class FEditorViewportClient : public IInputTarget
{
public:
	static constexpr float DefaultCameraSpeed = 5.0f;
	static constexpr float MinCameraSpeed = 0.1f;
	static constexpr float MaxCameraSpeed = 50.0f;

	explicit FEditorViewportClient(FViewport& InViewport);
	virtual ~FEditorViewportClient();

	virtual void Tick(float DeltaTime);

	virtual UWorld* GetWorld() const;
	FScene* GetScene() const;

	// Render State
	virtual FSceneView BuildSceneView();
	virtual std::optional<FSceneViewFamily> BuildSceneViewFamily();
	FShowFlags& GetShowFlags() { return ShowFlags; }

	// Input
	FInputReply OnInputEvent(const FInputEvent& Event) override;
	void OnKeyboardFocusLost() override;
	void OnMouseCaptureLost() override;

	// Camera
	FEditorViewportCamera& GetCamera() { return Camera; }
	const FEditorViewportCamera& GetCamera() const { return Camera; }
	void OnCameraStateChanged();
	void OnViewTransformChanged();

protected:
	FViewport& GetViewport() const { return Viewport; }

private:
	bool UpdateKeyState(const FKeyInputEvent& Event);
	bool IsKeyDown(EKeyboardKey Key) const;

	FViewport& Viewport;
	FShowFlags ShowFlags;

	FEditorViewportCamera Camera;
	TBitset<static_cast<SIZE_T>(EKeyboardKey::Count)> KeysDown;
	bool bRotatingCamera = false;
};
