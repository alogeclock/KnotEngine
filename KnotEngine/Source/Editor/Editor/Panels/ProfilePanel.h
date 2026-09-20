#pragma once

#include "Core/Profiling/CPUProfiler.h"
#include "Render/RHI/RenderTypes.h"

class FRenderSystem;

// 완료된 CPU Profile Frame을 주기적으로 집계하고 CPU 통계를 ImGui 표로 표시한다.
class FProfilePanel
{
public:
	explicit FProfilePanel(FRenderSystem& InRenderSystem);
	void Draw(float DeltaTime);

private:
	// 동일한 Profile ID의 Scope를 프레임에 걸쳐 누적하여 표시용 평균과 최대·최소를 보관한다.
	struct FCPUHistoryStat
	{
		ECPUProfileThread Thread = ECPUProfileThread::Game;
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
	FCPUProfileFrame DisplayedGameFrame;
	FCPUProfileFrame DisplayedRenderFrame;
	FGPUFrameStatistics DisplayedGPUStats;

	FRenderSystem& RenderSystem;

	float RefreshTimer = 0.0f;

	uint64 LastSampledGameFrameNumber = 0;
	uint64 LastSampledRenderFrameNumber = 0;
	uint64 LastSampledGPUFrameNumber = 0;
	bool bPaused = false;

	void Sample(const FCPUProfileFrame& Frame);
	void Refresh(const FCPUProfileFrame& GameFrame, const FCPUProfileFrame& RenderFrame);
	void DrawGPUStats() const;
	void DrawCPUStats(ECPUProfileThread Thread, const char* HeaderName, const char* TableName) const;
};
