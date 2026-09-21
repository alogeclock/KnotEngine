#pragma once

#include "Core/Geometry/Frustum.h"
#include "Core/Math/Matrix.h"
#include "Render/RHI/RenderTypes.h"

class FScene;

// Viewport가 Primitive 표면을 표시하는 방식을 지정한다.
enum class EViewMode : uint8
{
	Unlit,
	Wireframe,
	ShadedWireframe,
};

// 단일 카메라에서 Scene을 수집하고 렌더링하는 데 필요한 View 단위 데이터다.
struct FSceneView
{
	FMatrix ViewMatrix;
	FMatrix ProjectionMatrix;
	FMatrix ViewProjectionMatrix;
	FVector ViewOrigin;
	float FarClip = 0.0f;
	FFrustum Frustum;
	FRenderViewport Viewport;
	EViewMode ViewMode = EViewMode::Unlit;
	
	int32 ForcedLODIndex = -1;
	TStaticArray<float, 4> LODSteps = { 0.15f, 0.08f, 0.05f, 0.02f };
};

// Shader Stage의 b0에 바인딩하는 View 단위 공용 상수다.
struct alignas(16) FViewConstants
{
	FMatrix ViewProjection;
	FMatrix InverseViewProjection;
	FVector ViewOrigin;
	float FarClip;
};
static_assert(sizeof(FViewConstants) == 144);

// 카메라에서 표시할 대상을 플래그 형태로 구별한다.
struct FShowFlags
{
	bool bPrimitive = false;
	bool bAxis = false;
	bool bGrid = false;
	bool bBounds = false;
};

struct FSceneRenderTarget
{
	FTextureHandle SceneColor;   // Gamma Correction 이전 Linear Color를 저장하며, Post Process Pass가 SRV로 읽는 Target이다.
	FTextureHandle DisplayColor; // Post Process 결과를 저장하며, Viewport Panel이 표시하는 최종 Target이다.
	FTextureHandle SelectionDepth; // 선택된 Primitive의 실루엣과 Depth를 함께 저장하는 Editor 전용 Target이다.
	FTextureHandle Depth;
	uint32 Width = 0;
	uint32 Height = 0;
};

// 제출 후 View 배열을 변경하지 않는다. Scene과 타깃은 프레임 완료까지 살아 있어야 한다.
struct FSceneViewFamily
{
	FScene* Scene = nullptr;
	TArray<FSceneView> Views;
	FSceneRenderTarget RenderTarget;
	FShowFlags ShowFlags;
};
