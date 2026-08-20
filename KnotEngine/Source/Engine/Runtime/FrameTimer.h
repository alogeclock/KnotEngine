#pragma once

#include <chrono>

// 프레임 간 경과 시간과 누적 시간, FPS 통계 및 최대 FPS 제한을 관리한다.
class FFrameTimer final
{
public:
	FFrameTimer();

	void Reset();
	void Tick();

	float GetDeltaTime() const { return DeltaTime; }
	double GetTotalTime() const { return TotalTime; }

	float GetFPS() const { return DeltaTime > 0.0f ? 1.0f / DeltaTime : 0.0f; }
	float GetDisplayFPS() const { return SmoothedFPS; }
	float GetFrameTimeMs() const { return DeltaTime * 1000.0f; }
	float GetTimeSinceLastTick() const;

	void SetMaxFPS(float InMaxFPS);
	float GetMaxFPS() const { return MaxFPS; }

private:
	using Clock = std::chrono::steady_clock;

	Clock::time_point LastTime = {};
	float DeltaTime = 0.0f;
	double TotalTime = 0.0;

	float MaxFPS = 0.0f;
	float TargetFrameTime = 0.0f;

	float SmoothedFPS = 0.0f;
	bool bIsFirstTick = true;
};
