#include "ViewportCameraTransform.h"

#include "Core/Math/Matrix.h"

#include <algorithm>
#include <cmath>

void FViewportCameraTransform::TranslateWorld(const FVector& WorldDelta)
{
	ViewLocation += WorldDelta;
}

void FViewportCameraTransform::TranslateLocal(const FVector& LocalDelta)
{
	// local Roll/Pitch 뒤 world Yaw를 적용한다. 양의 Pitch는 +Z를 바라본다.
	const FMatrix RotationX = FMatrix::MakeRotationX(KMath::ToRadian(ViewRotation.Roll));
	const FMatrix RotationY = FMatrix::MakeRotationY(KMath::ToRadian(-ViewRotation.Pitch));
	const FMatrix RotationZ = FMatrix::MakeRotationZ(KMath::ToRadian(ViewRotation.Yaw));
	const FMatrix Rotation = RotationX * RotationY * RotationZ;

	const FVector Forward = Rotation.GetScaledAxis(EAxis::X);
	const FVector Right = Rotation.GetScaledAxis(EAxis::Y);
	const FVector Up = Rotation.GetScaledAxis(EAxis::Z);
	TranslateWorld(Forward * LocalDelta.X + Right * LocalDelta.Y + Up * LocalDelta.Z);
}

void FViewportCameraTransform::Rotate(float DeltaYaw, float DeltaPitch)
{
	static constexpr float MaxPitch = 89.0f;
	ViewRotation.Yaw = FRotator::NormalizeAxis(ViewRotation.Yaw + DeltaYaw);
	ViewRotation.Pitch = std::clamp(ViewRotation.Pitch + DeltaPitch, -MaxPitch, MaxPitch);
	ViewRotation.Roll = 0.0f;
}

// 같은 위치는 무시하고, 수직 방향은 기존 Yaw를 유지한다. 정확히 바라볼 수 있도록 pitch clamp를 적용하지 않는다.
void FViewportCameraTransform::LookAt(const FVector& Target)
{
	FVector Direction = Target - ViewLocation;
	if (!Direction.Normalize())
	{
		return;
	}

	const float HorizontalLength = std::hypot(Direction.X, Direction.Y);
	if (HorizontalLength > KMath::Epsilon)
	{
		ViewRotation.Yaw = FRotator::NormalizeAxis(KMath::ToDegree(std::atan2(Direction.Y, Direction.X)));
	}
	ViewRotation.Pitch = KMath::ToDegree(std::atan2(Direction.Z, HorizontalLength));
	ViewRotation.Roll = 0.0f;
}
