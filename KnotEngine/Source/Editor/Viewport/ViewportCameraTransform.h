#pragma once

#include "Core/Math/Rotator.h"
#include "Core/Math/Vector.h"

// 부모가 없는 에디터 카메라의 변환과 투영 값이다. 위치는 cm, 회전은 degree를 사용한다.
struct FViewportCameraTransform
{
	FVector ViewLocation = FVector(-5.0f, 0.0f, 0.0f);
	FRotator ViewRotation = FRotator::ZeroRotator;

	float OrthoZoom = 10.0f; // 직교 화면 폭 (cm)
	float FOV = 3.14159265358979f / 3.0f; // 수직 시야각 (radians)
	float AspectRatio = 16.0f / 9.0f;
	float NearClip = 0.1f;
	float FarClip = 10000.0f;
	bool bIsOrtho = false;

	void TranslateWorld(const FVector& WorldDelta);
	void TranslateLocal(const FVector& LocalDelta);
	void Rotate(float DeltaYaw, float DeltaPitch);
	void LookAt(const FVector& Target);
};
