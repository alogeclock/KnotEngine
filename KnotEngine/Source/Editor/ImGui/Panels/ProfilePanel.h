#pragma once

#include "Core/Profiling/CPUProfiler.h"

// 완료된 CPU Profile Frame을 주기적으로 집계하고 CPU 통계를 ImGui 표로 표시한다.
class FProfilePanel
{
public:
	void Draw(float DeltaTime);

private:
	// 동일한 Profile ID의 Scope를 프레임에 걸쳐 누적하여 표시용 평균과 최대·최소를 보관한다.
	struct FCPUHistoryStat
	{
		FString Category;
		FString Name;

		double TotalTimeMs = 0.0;
		double AverageTimeMs = 0.0;
		double MaxTimeMs = 0.0;
		double MinTimeMs = 0.0;
		double AccumulatedTimeMs = 0.0;

		uint64 SampleCount = 0;
		uint32 CallCount = 0;
	};

	static constexpr float RefreshInterval = 0.25f;

	TArray<FCPUHistoryStat> HistoryStats;
	TArray<FCPUHistoryStat> DisplayedStats;
	FCPUProfileFrame DisplayedFrame;

	float RefreshTimer = 0.0f;

	uint64 AccumulatedFrameCount = 0;
	double AccumulatedFrameTimeMs = 0.0;
	uint64 LastSampledFrameNumber = 0;
	bool bPaused = false;

	void Sample(const FCPUProfileFrame& Frame);
	void Refresh(const FCPUProfileFrame& Frame);
	void DrawCPUStats() const;
};
