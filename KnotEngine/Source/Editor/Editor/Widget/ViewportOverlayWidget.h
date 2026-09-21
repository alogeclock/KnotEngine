#pragma once

#include "Core/CoreTypes.h"

// Console 명령과 Viewport 통계 오버레이가 공유하는 표시 상태다.
struct FViewportStatState
{
	bool bShowFPS = false;
	bool bShowMemory = false;
};

// Viewport 위에 표시할 FPS를 집계하고 활성화된 통계를 반투명 창으로 그린다.
class FViewportOverlayWidget
{
public:
	explicit FViewportOverlayWidget(const FViewportStatState& InState);

	void Tick(float DeltaTime);
	void Draw() const;

private:
	const FViewportStatState& State;

	float SampleTime = 0.0f;
	float DisplayedFPS = 0.0f;
	float DisplayedFrameTimeMs = 0.0f;
	uint32 SampleCount = 0;
};
