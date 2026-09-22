#pragma once

#include "Core/CoreTypes.h"
#include "Core/Math/Rotator.h"
#include "Core/Math/Vector.h"

// 부모가 없는 에디터 카메라의 변환과 투영 값이다. 위치는 cm, 회전은 degree를 사용한다.
struct FEditorViewportCameraTransform
{
	FVector ViewLocation = FVector(-200.0f, 200.0f, 250.0f);
	FRotator ViewRotation = FRotator(-45.0f, -45.0f, 0.0f);

	float OrthoZoom = 10.0f; // 직교 화면 폭 (cm)
	float FOV = 60.0f; // 수직 시야각 (degree)
	float AspectRatio = 16.0f / 9.0f;
	float NearClip = 5.0f;
	float FarClip = 100000.0f;
	bool bIsOrtho = false;

	void TranslateWorld(const FVector& WorldDelta);
	void TranslateLocal(const FVector& LocalDelta);
	void Rotate(float DeltaYaw, float DeltaPitch);
	void LookAt(const FVector& Target);
};

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

struct FEditorViewportCamera
{
	FEditorViewportCameraTransform ViewTransform;
	EEditorViewportViewMode ViewMode = EEditorViewportViewMode::Perspective;	
	float Sensitivity = 1.0f;
};
