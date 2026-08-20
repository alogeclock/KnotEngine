#include "Runtime/FrameTimer.h"

#include <cmath>
#include <thread>

// 생성과 동시에 사용할 수 있도록 타이머 기준 시각과 통계를 설정한다.
FFrameTimer::FFrameTimer()
{
	Reset();
}

// 타이머 기준 시각을 설정하고 모든 프레임 통계를 초기화한다.
void FFrameTimer::Reset()
{
	LastTime = Clock::now();
	DeltaTime = 0.0f;
	TotalTime = 0.0;
	SmoothedFPS = 0.0f;
	bIsFirstTick = true;
}

// 최대 FPS 제한을 적용한 뒤 실제 프레임 경과 시간과 FPS 통계를 갱신한다.
void FFrameTimer::Tick()
{
	// Frame Capping: 1) Target Frame Time이 0보다 작아질 때까지 이번 프레임의 목표 완료 시점을 계산한다.
	Clock::time_point CurrentTime = Clock::now();
	if (TargetFrameTime > 0.0f)
	{
		const Clock::duration TargetDuration = std::chrono::duration_cast<Clock::duration>(std::chrono::duration<float>(TargetFrameTime));
		const Clock::time_point TargetTime = LastTime + TargetDuration;

		// 2) 현재 시각이 목표 시점보다 이르다면, 스레드를 대기 상태로 두고 남은 시간을 소모한다.
		if (CurrentTime < TargetTime)
		{
			std::this_thread::sleep_until(TargetTime);
			CurrentTime = Clock::now();
		}
	}

	const double ElapsedTime = std::chrono::duration<double>(CurrentTime - LastTime).count();
	LastTime = CurrentTime;
	DeltaTime = static_cast<float>(ElapsedTime);
	TotalTime += ElapsedTime;

	// Reset() 직후 첫 Tick()이 너무 빨리 호출되어 FPS가 급격하게 튀지 않도록 방지
	if (bIsFirstTick)
	{
		bIsFirstTick = false;
		return;
	}

	const float CurrentFPS = GetFPS();
	if (CurrentFPS <= 0.0f)
	{
		return;
	}

	if (SmoothedFPS <= 0.0f)
	{
		SmoothedFPS = CurrentFPS;
		return;
	}

	// Exponential Decay: FPS가 매 프레임 급격하게 튀는 것을 방지하기 위해 지수 감쇠 공식을 적용
	static constexpr float SmoothingRate = 5.0f;
	const float BlendFactor = 1.0f - std::exp(-SmoothingRate * DeltaTime); // 프레임률 변동과 무관하게 일정 속도로 수렴
	SmoothedFPS += (CurrentFPS - SmoothedFPS) * BlendFactor;
}

// 마지막 Tick 이후 실제로 경과한 시간을 초 단위로 반환한다.
float FFrameTimer::GetTimeSinceLastTick() const
{
	return std::chrono::duration<float>(Clock::now() - LastTime).count();
}

// 양수 FPS는 프레임 제한으로 설정하고 0 이하이거나 유효하지 않은 값은 제한 해제로 처리한다.
void FFrameTimer::SetMaxFPS(float InMaxFPS)
{
	if (!std::isfinite(InMaxFPS) || InMaxFPS <= 0.0f)
	{
		MaxFPS = 0.0f;
		TargetFrameTime = 0.0f;
		return;
	}

	MaxFPS = InMaxFPS;
	TargetFrameTime = 1.0f / MaxFPS;
}
