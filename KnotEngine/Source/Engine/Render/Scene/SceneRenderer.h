#pragma once

#include "Render/Scene/SceneView.h"

class URenderer;
struct FPrimitiveSceneProxy;

// ViewFamily 한 개를 처리하는 메인 스레드 지역 객체. UI와 Present는 상위 프레임이 담당한다.
class ENGINE_API FSceneRenderer
{
public:
	explicit FSceneRenderer(const FSceneViewFamily& InViewFamily);
	void Render(URenderer& Renderer);

private:
	void CullView(const FSceneView& View);

	const FSceneViewFamily& ViewFamily;
	TArray<const FPrimitiveSceneProxy*> VisiblePrimitives;
};
