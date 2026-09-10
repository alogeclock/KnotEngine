#include "EditorViewportClient.h"

#include "Core/Math/Matrix.h"
#include "Runtime/Engine.h"
#include "Viewport/Viewport.h"

#include <algorithm>
#include <cmath>
#include <variant>

FEditorViewportClient::FEditorViewportClient(FViewport& InViewport)
    : Viewport(InViewport)
{
}

FEditorViewportClient::~FEditorViewportClient() = default;

void FEditorViewportClient::Tick(float DeltaTime)
{
	if (CameraState.ViewMode == EEditorViewportViewMode::Perspective && !bRotatingCamera)
	{
		return;
	}

	const FRotator& ViewRotation = CameraState.ViewTransform.ViewRotation;
	const FMatrix RotationX = FMatrix::MakeRotationX(KMath::ToRadian(ViewRotation.Roll));
	const FMatrix RotationY = FMatrix::MakeRotationY(KMath::ToRadian(-ViewRotation.Pitch));
	const FMatrix RotationZ = FMatrix::MakeRotationZ(KMath::ToRadian(ViewRotation.Yaw));
	const FMatrix Rotation = RotationX * RotationY * RotationZ;
	const FVector Forward = Rotation.GetScaledAxis(EAxis::X);
	const FVector Right = Rotation.GetScaledAxis(EAxis::Y);
	const FVector Up = Rotation.GetScaledAxis(EAxis::Z);

	FVector MoveDirection = FVector::ZeroVector;
	const FVector ForwardAxis = CameraState.ViewMode == EEditorViewportViewMode::Perspective ? Forward : Up;
	MoveDirection += ForwardAxis * static_cast<float>(IsKeyDown(EKeyboardKey::W) - IsKeyDown(EKeyboardKey::S));
	MoveDirection += Right * static_cast<float>(IsKeyDown(EKeyboardKey::D) - IsKeyDown(EKeyboardKey::A));
	if (CameraState.ViewMode == EEditorViewportViewMode::Perspective)
	{
		MoveDirection += FVector::UpVector * static_cast<float>(IsKeyDown(EKeyboardKey::E) - IsKeyDown(EKeyboardKey::Q));
	}
	if (!MoveDirection.Normalize())
	{
		return;
	}

	const FVector MoveDelta = MoveDirection * CameraState.CameraSpeed * DeltaTime;
	CameraState.ViewTransform.TranslateWorld(MoveDelta);
}

void FEditorViewportClient::Draw(URenderer& Renderer)
{
	if (!Viewport.IsValid())
	{
		return;
	}

	DrawViewport(Renderer);
}

UWorld* FEditorViewportClient::GetWorld() const
{
	return GEngine ? GEngine->GetWorld() : nullptr;
}

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
		if (PointerEvent->Button == EMouseButton::Right && CameraState.ViewMode == EEditorViewportViewMode::Perspective)
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
		CameraState.ViewTransform.Rotate(PointerEvent->Delta.X * LookSensitivity, -PointerEvent->Delta.Y * LookSensitivity);
		return FInputReply::Handled();
	}
	if (PointerEvent->Type == EPointerInputEventType::Wheel)
	{
		const float WheelDelta = PointerEvent->WheelDelta.Y;
		if (CameraState.ViewMode != EEditorViewportViewMode::Perspective)
		{
			static constexpr float Step = 0.85f;
			static constexpr float MinOrthoZoom = 0.1f;
			static constexpr float MaxOrthoZoom = 10000.0f;
			CameraState.ViewTransform.OrthoZoom = std::clamp(CameraState.ViewTransform.OrthoZoom * std::pow(Step, WheelDelta), MinOrthoZoom, MaxOrthoZoom);
		}
		else
		{
			static constexpr float WheelMoveMultiplier = 0.2f;
			CameraState.ViewTransform.TranslateLocal(FVector(WheelDelta * CameraState.CameraSpeed * WheelMoveMultiplier, 0.0f, 0.0f));
		}
		return FInputReply::Handled();
	}

	return FInputReply::Unhandled();
}

void FEditorViewportClient::OnKeyboardFocusLost()
{
	KeysDown.reset();
}

void FEditorViewportClient::OnMouseCaptureLost()
{
	bRotatingCamera = false;
}

void FEditorViewportClient::OnCameraStateChanged()
{
	FViewportCameraTransform& ViewTransform = CameraState.ViewTransform;
	CameraState.CameraSpeed = std::clamp(CameraState.CameraSpeed, MinCameraSpeed, MaxCameraSpeed);
	ViewTransform.bIsOrtho = CameraState.ViewMode != EEditorViewportViewMode::Perspective;
	if (!ViewTransform.bIsOrtho)
	{
		return;
	}

	static constexpr float ViewDistance = 500.0f;
	switch (CameraState.ViewMode)
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
	CameraState.ViewTransform.ViewRotation.Normalize();
}

FMatrix FEditorViewportClient::GetViewProjectionMatrix()
{
	const FRenderViewport ViewportInfo = Viewport.GetRenderViewport();
	check(ViewportInfo.Width > 0.0f && ViewportInfo.Height > 0.0f);

	FViewportCameraTransform& Camera = CameraState.ViewTransform;
	Camera.AspectRatio = ViewportInfo.Width / ViewportInfo.Height;
	check(Camera.NearClip > 0.0f && Camera.FarClip > Camera.NearClip);

	const FRotator& ViewRotation = Camera.ViewRotation;
	const FMatrix RotationX = FMatrix::MakeRotationX(KMath::ToRadian(ViewRotation.Roll));
	const FMatrix RotationY = FMatrix::MakeRotationY(KMath::ToRadian(-ViewRotation.Pitch));
	const FMatrix RotationZ = FMatrix::MakeRotationZ(KMath::ToRadian(ViewRotation.Yaw));
	const FMatrix Rotation = RotationX * RotationY * RotationZ;

	const FVector Forward = Rotation.GetScaledAxis(EAxis::X);
	const FVector Up = Rotation.GetScaledAxis(EAxis::Z);
	const FMatrix View = FMatrix::MakeLookAt(Camera.ViewLocation, Camera.ViewLocation + Forward, Up);

	if (!Camera.bIsOrtho)
	{
		const FMatrix Projection = FMatrix::MakePerspectiveFov(Camera.FOV, Camera.AspectRatio, Camera.NearClip, Camera.FarClip);
		return View * Projection;
	}

	const FMatrix Projection = FMatrix::MakeOrthographic(Camera.OrthoZoom, Camera.OrthoZoom / Camera.AspectRatio, Camera.NearClip, Camera.FarClip);
	return View * Projection;
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
