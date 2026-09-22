#include "EditorViewportClient.h"

#include "Core/Math/Matrix.h"
#include "Runtime/Engine.h"
#include "World/World.h"
#include "Viewport/Viewport.h"

#include <algorithm>
#include <cmath>
#include <variant>

FEditorViewportClient::FEditorViewportClient(FViewport& InViewport)
    : Viewport(InViewport)
{
	ShowFlags.bPrimitive = true;
	ShowFlags.bAxis = true;
	ShowFlags.bGrid = true;
	ShowFlags.bBounds = false;
}

FEditorViewportClient::~FEditorViewportClient() = default;

void FEditorViewportClient::Tick(float DeltaTime)
{
	if (Camera.ViewMode == EEditorViewportViewMode::Perspective && !bRotatingCamera)
	{
		return;
	}

	const FRotator& ViewRotation = Camera.ViewTransform.ViewRotation;
	const FMatrix RotationX = FMatrix::MakeRotationX(KMath::ToRadian(ViewRotation.Roll));
	const FMatrix RotationY = FMatrix::MakeRotationY(KMath::ToRadian(-ViewRotation.Pitch));
	const FMatrix RotationZ = FMatrix::MakeRotationZ(KMath::ToRadian(ViewRotation.Yaw));
	const FMatrix Rotation = RotationX * RotationY * RotationZ;
	const FVector Forward = Rotation.GetScaledAxis(EAxis::X);
	const FVector Right = Rotation.GetScaledAxis(EAxis::Y);
	const FVector Up = Rotation.GetScaledAxis(EAxis::Z);

	FVector MoveDirection = FVector::ZeroVector;
	const FVector ForwardAxis = Camera.ViewMode == EEditorViewportViewMode::Perspective ? Forward : Up;
	MoveDirection += ForwardAxis * static_cast<float>(IsKeyDown(EKeyboardKey::W) - IsKeyDown(EKeyboardKey::S));
	MoveDirection += Right * static_cast<float>(IsKeyDown(EKeyboardKey::D) - IsKeyDown(EKeyboardKey::A));
	if (Camera.ViewMode == EEditorViewportViewMode::Perspective)
	{
		MoveDirection += FVector::UpVector * static_cast<float>(IsKeyDown(EKeyboardKey::E) - IsKeyDown(EKeyboardKey::Q));
	}
	if (!MoveDirection.Normalize())
	{
		return;
	}

	const FVector MoveDelta = MoveDirection * CameraMoveSpeed * Camera.Sensitivity * DeltaTime;
	Camera.ViewTransform.TranslateWorld(MoveDelta);
}

FScene* FEditorViewportClient::GetScene() const
{
	UWorld* World = GetWorld();
	return World ? &World->GetScene() : nullptr;
}

std::optional<FSceneViewFamily> FEditorViewportClient::BuildSceneViewFamily()
{
	FScene* Scene = GetScene();
	if (!Viewport.IsValid() || !Scene)
	{
		return std::nullopt;
	}
	FSceneViewFamily Family;
	Family.Scene = Scene;
	Family.RenderTarget = {
		Viewport.GetSceneColorTarget(),
		Viewport.GetDisplayColorTarget(),
		Viewport.GetSelectionDepthTarget(),
		Viewport.GetDepthTarget(),
		Viewport.GetWidth(),
		Viewport.GetHeight()
	};
	Family.ShowFlags = ShowFlags;
	Family.Views.push_back(BuildSceneView());
	Family.Views.back().ViewMode = ViewMode;
	return Family;
}

UWorld* FEditorViewportClient::GetWorld() const
{
	return GEngine ? GEngine->GetWorld() : nullptr;
}

// Application Client 좌표의 Viewport 이미지 영역을 입력 좌표 변환에 사용하도록 저장한다.
void FEditorViewportClient::SetInputRect(const FVector2& Position, const FVector2& Size)
{
	InputRectPosition = Position;
	InputRectSize = Size;
}

// Application Client 좌표를 Offscreen Render Target의 Pixel 좌표로 변환한다.
bool FEditorViewportClient::GetViewportPixelPosition(const FVector2& InputPosition, FVector2& OutPixelPosition) const
{
	if (InputRectSize.X <= 0.0f || InputRectSize.Y <= 0.0f || !Viewport.IsValid())
	{
		return false;
	}

	const FVector2 LocalPosition = InputPosition - InputRectPosition;

	OutPixelPosition.X = LocalPosition.X * static_cast<float>(Viewport.GetWidth()) / InputRectSize.X;
	OutPixelPosition.Y = LocalPosition.Y * static_cast<float>(Viewport.GetHeight()) / InputRectSize.Y;
	return true;
}

// 클라이언트 좌표가 Viewport 이미지 영역 안에 있는지 검사한다.
bool FEditorViewportClient::ContainsInputPosition(const FVector2& InputPosition) const
{
	const FVector2 LocalPosition = InputPosition - InputRectPosition;
	return LocalPosition.X >= 0.0f && LocalPosition.Y >= 0.0f && LocalPosition.X < InputRectSize.X && LocalPosition.Y < InputRectSize.Y;
}

// 라우팅된 키와 포인터 이벤트를 카메라 이동·회전·줌 입력으로 처리한다.
FInputReply FEditorViewportClient::OnInputEvent(const FInputEvent& Event)
{
	if (const FKeyInputEvent* KeyEvent = std::get_if<FKeyInputEvent>(&Event))
	{
		return UpdateKeyState(*KeyEvent) ? FInputReply::Handled() : FInputReply::Unhandled();
	}

	const FPointerInputEvent* PointerEvent = std::get_if<FPointerInputEvent>(&Event);
	if (!PointerEvent)
	{
		return FInputReply::Unhandled();
	}

	if (PointerEvent->Type == EPointerInputEventType::ButtonDown)
	{
		FInputReply Reply = FInputReply::Handled().SetKeyboardFocus();
		if (PointerEvent->Button == EMouseButton::Right && Camera.ViewMode == EEditorViewportViewMode::Perspective)
		{
			bRotatingCamera = true;
			Reply.CaptureMouse();
		}
		return Reply;
	}
	if (PointerEvent->Type == EPointerInputEventType::ButtonUp && PointerEvent->Button == EMouseButton::Right)
	{
		bRotatingCamera = false;
		return FInputReply::Handled().ReleaseMouse();
	}
	if (PointerEvent->Type == EPointerInputEventType::MouseMoved && bRotatingCamera)
	{
		static constexpr float LookSensitivity = 0.15f;
		Camera.ViewTransform.Rotate(PointerEvent->Delta.X * LookSensitivity, -PointerEvent->Delta.Y * LookSensitivity);
		return FInputReply::Handled();
	}
	if (PointerEvent->Type == EPointerInputEventType::CursorMoved && bRotatingCamera)
	{
		return WrapCameraCursor(PointerEvent->Position);
	}
	if (PointerEvent->Type == EPointerInputEventType::Wheel)
	{
		const float WheelDelta = PointerEvent->WheelDelta.Y;
		if (Camera.ViewMode != EEditorViewportViewMode::Perspective)
		{
			static constexpr float Step = 0.85f;
			static constexpr float MinOrthoZoom = 0.1f;
			static constexpr float MaxOrthoZoom = 10000.0f;
			Camera.ViewTransform.OrthoZoom = std::clamp(Camera.ViewTransform.OrthoZoom * std::pow(Step, WheelDelta), MinOrthoZoom, MaxOrthoZoom);
		}
		else
		{
			static constexpr float WheelMoveMultiplier = 0.2f;
			Camera.ViewTransform.TranslateLocal(FVector(WheelDelta * CameraMoveSpeed * Camera.Sensitivity * WheelMoveMultiplier, 0.0f, 0.0f));
		}
		return FInputReply::Handled();
	}

	return FInputReply::Unhandled();
}

// Viewport를 벗어난 카메라 드래그 커서를 반대편 내부로 옮기도록 요청한다.
FInputReply FEditorViewportClient::WrapCameraCursor(const FVector2& Position) const
{
	if (InputRectSize.X <= 2.0f || InputRectSize.Y <= 2.0f)
	{
		return FInputReply::Handled();
	}

	FVector2 WrappedPosition = Position;
	const FVector2 MaxPosition = InputRectPosition + InputRectSize;
	if (Position.X < InputRectPosition.X)
	{
		WrappedPosition.X = MaxPosition.X - 1.0f;
	}
	else if (Position.X >= MaxPosition.X)
	{
		WrappedPosition.X = InputRectPosition.X + 1.0f;
	}
	if (Position.Y < InputRectPosition.Y)
	{
		WrappedPosition.Y = MaxPosition.Y - 1.0f;
	}
	else if (Position.Y >= MaxPosition.Y)
	{
		WrappedPosition.Y = InputRectPosition.Y + 1.0f;
	}
	return WrappedPosition == Position ? FInputReply::Handled() : FInputReply::Handled().WarpCursor(WrappedPosition);
}

// 키보드 포커스를 잃으면 유지 중인 이동 키와 카메라 회전을 초기화한다.
void FEditorViewportClient::OnKeyboardFocusLost()
{
	KeysDown.reset();
	bRotatingCamera = false;
}

void FEditorViewportClient::OnMouseCaptureLost()
{
	bRotatingCamera = false;
}

void FEditorViewportClient::OnCameraStateChanged()
{
	FEditorViewportCameraTransform& ViewTransform = Camera.ViewTransform;
	Camera.Sensitivity = std::clamp(Camera.Sensitivity, MinCameraSensitivity, MaxCameraSensitivity);
	ViewTransform.bIsOrtho = Camera.ViewMode != EEditorViewportViewMode::Perspective;
	if (!ViewTransform.bIsOrtho)
	{
		return;
	}

	static constexpr float ViewDistance = 500.0f;
	switch (Camera.ViewMode)
	{
	case EEditorViewportViewMode::Top:
		ViewTransform.ViewLocation = FVector(0.0f, 0.0f, ViewDistance);
		ViewTransform.ViewRotation = FRotator(-90.0f, 0.0f, 0.0f);
		break;
	case EEditorViewportViewMode::Bottom:
		ViewTransform.ViewLocation = FVector(0.0f, 0.0f, -ViewDistance);
		ViewTransform.ViewRotation = FRotator(90.0f, 0.0f, 180.0f);
		break;
	case EEditorViewportViewMode::Left:
		ViewTransform.ViewLocation = FVector(0.0f, -ViewDistance, 0.0f);
		ViewTransform.ViewRotation = FRotator(0.0f, 90.0f, 0.0f);
		break;
	case EEditorViewportViewMode::Right:
		ViewTransform.ViewLocation = FVector(0.0f, ViewDistance, 0.0f);
		ViewTransform.ViewRotation = FRotator(0.0f, -90.0f, 0.0f);
		break;
	case EEditorViewportViewMode::Front:
		ViewTransform.ViewLocation = FVector(-ViewDistance, 0.0f, 0.0f);
		ViewTransform.ViewRotation = FRotator::ZeroRotator;
		break;
	case EEditorViewportViewMode::Back:
		ViewTransform.ViewLocation = FVector(ViewDistance, 0.0f, 0.0f);
		ViewTransform.ViewRotation = FRotator(0.0f, 180.0f, 0.0f);
		break;
	case EEditorViewportViewMode::Perspective:
		break;
	}
	bRotatingCamera = false;
}

void FEditorViewportClient::OnViewTransformChanged()
{
	Camera.ViewTransform.ViewRotation.Normalize();
}

FSceneView FEditorViewportClient::BuildSceneView()
{
	const FRenderViewport ViewportInfo = Viewport.GetRenderViewport();
	check(ViewportInfo.Width > 0.0f && ViewportInfo.Height > 0.0f);

	FEditorViewportCameraTransform& Transform = Camera.ViewTransform;
	Transform.AspectRatio = ViewportInfo.Width / ViewportInfo.Height;
	check(Transform.NearClip > 0.0f && Transform.FarClip > Transform.NearClip);

	const FRotator& ViewRotation = Transform.ViewRotation;
	const FMatrix RotationX = FMatrix::MakeRotationX(KMath::ToRadian(ViewRotation.Roll));
	const FMatrix RotationY = FMatrix::MakeRotationY(KMath::ToRadian(-ViewRotation.Pitch));
	const FMatrix RotationZ = FMatrix::MakeRotationZ(KMath::ToRadian(ViewRotation.Yaw));
	const FMatrix Rotation = RotationX * RotationY * RotationZ;

	const FVector Forward = Rotation.GetScaledAxis(EAxis::X);
	const FVector Up = Rotation.GetScaledAxis(EAxis::Z);
	const FMatrix View = FMatrix::MakeLookAt(Transform.ViewLocation, Transform.ViewLocation + Forward, Up);

	const FMatrix Projection = !Transform.bIsOrtho
		? FMatrix::MakePerspectiveFov(KMath::ToRadian(Transform.FOV), Transform.AspectRatio, Transform.NearClip, Transform.FarClip)
		: FMatrix::MakeOrthographic(Transform.OrthoZoom, Transform.OrthoZoom / Transform.AspectRatio, Transform.NearClip, Transform.FarClip);
	FSceneView SceneView;
	SceneView.ViewMatrix = View;
	SceneView.ProjectionMatrix = Projection;
	SceneView.ViewProjectionMatrix = View * Projection;
	SceneView.ViewOrigin = Transform.ViewLocation;
	SceneView.FarClip = Transform.FarClip;
	SceneView.Viewport = ViewportInfo;
	SceneView.Frustum.UpdateFromCamera(SceneView.ViewProjectionMatrix);
	return SceneView;
}

bool FEditorViewportClient::UpdateKeyState(const FKeyInputEvent& Event)
{
	switch (Event.Key)
	{
	case EKeyboardKey::W:
	case EKeyboardKey::S:
	case EKeyboardKey::A:
	case EKeyboardKey::D:
	case EKeyboardKey::Q:
	case EKeyboardKey::E:
		break;
	default:
		return false;
	}

	// Repeat는 소비하되, 포커스 상실로 취소된 이동을 다시 시작하지 않는다.
	if (!Event.bDown || !Event.bRepeat)
	{
		KeysDown.set(static_cast<SIZE_T>(Event.Key), Event.bDown);
	}
	return true;
}

bool FEditorViewportClient::IsKeyDown(EKeyboardKey Key) const
{
	check(Key != EKeyboardKey::Unknown && static_cast<SIZE_T>(Key) < KeysDown.size());
	return KeysDown[static_cast<SIZE_T>(Key)];
}
