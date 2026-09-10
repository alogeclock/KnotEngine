#pragma once

#include "Core/CoreTypes.h"
#include "Core/Math/Rotator.h"
#include "Core/Math/Vector.h"
#include "Input/InputRouter.h"
#include "ViewportCameraTransform.h"

struct FMatrix;
class FViewport;
class URenderer;
class UWorld;

enum class EEditorViewportViewMode : uint8
{
	Perspective,
	Top,
	Bottom,
	Left,
	Right,
	Front,
	Back,
};

struct FEditorViewportCameraState
{
	FViewportCameraTransform ViewTransform;
	EEditorViewportViewMode ViewMode = EEditorViewportViewMode::Perspective; // TO-DO: AssetEditor, LevelEditor 일반화
	float CameraSpeed = 5.0f;
};

// EditorViewportClient는 공통 에디터 카메라와 입력을 관리하고 구체적인 Scene draw를 파생 클래스에 위임한다.
class FEditorViewportClient : public IInputTarget
{
public:
	static constexpr float DefaultCameraSpeed = 5.0f;
	static constexpr float MinCameraSpeed = 0.1f;
	static constexpr float MaxCameraSpeed = 20.0f;

	explicit FEditorViewportClient(FViewport& InViewport);
	virtual ~FEditorViewportClient();

	virtual void Tick(float DeltaTime);
	void Draw(URenderer& Renderer);
	UWorld* GetWorld() const;
	FInputReply OnInputEvent(const FInputEvent& Event) override;
	void OnKeyboardFocusLost() override;
	void OnMouseCaptureLost() override;

	FEditorViewportCameraState& GetCameraState() { return CameraState; }
	const FEditorViewportCameraState& GetCameraState() const { return CameraState; }
	void OnCameraStateChanged();
	void OnViewTransformChanged();

protected:
	FViewport& GetViewport() const { return Viewport; }
	FMatrix GetViewProjectionMatrix();
	virtual void DrawViewport(URenderer& Renderer) = 0;

private:
	bool UpdateKeyState(const FKeyInputEvent& Event);
	bool IsKeyDown(EKeyboardKey Key) const;

	FViewport& Viewport;

	FEditorViewportCameraState CameraState;
	TBitset<static_cast<SIZE_T>(EKeyboardKey::Count)> KeysDown;
	bool bRotatingCamera = false;
};
