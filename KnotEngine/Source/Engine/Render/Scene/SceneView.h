#pragma once

#include "Core/Geometry/Frustum.h"
#include "Core/Math/Matrix.h"
#include "Render/RHI/RenderTypes.h"

class FScene;

struct FSceneView
{
	FMatrix ViewMatrix;
	FMatrix ProjectionMatrix;
	FMatrix ViewProjectionMatrix;
	FVector ViewOrigin;
	float FarClip = 0.0f;
	FFrustum Frustum;
	FRenderViewport Viewport;
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
