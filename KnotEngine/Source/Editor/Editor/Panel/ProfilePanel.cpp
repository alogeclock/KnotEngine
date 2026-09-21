#include "Editor/Panel/ProfilePanel.h"

#include "Render/RenderSystem.h"

#include <algorithm>
#include <imgui.h>

FProfilePanel::FProfilePanel(FRenderSystem& InRenderSystem)
	: RenderSystem(InRenderSystem)
{
}

// 최신 완료 프레임을 샘플링하고 갱신 주기에 맞춰 Profile Panel을 그린다.
void FProfilePanel::Draw(float DeltaTime)
{
	RefreshTimer -= DeltaTime;
	const FCPUProfileFrame GameFrame = FCPUProfiler::GetLastFrame(ECPUProfileThread::Game);
	const FCPUProfileFrame RenderFrame = FCPUProfiler::GetLastFrame(ECPUProfileThread::Render);
	bool bRefreshed = false;
	if (!bPaused && GameFrame.FrameNumber != 0 && GameFrame.FrameNumber != LastSampledGameFrameNumber)
	{
		Sample(GameFrame);
		LastSampledGameFrameNumber = GameFrame.FrameNumber;
	}
	if (!bPaused && RenderFrame.FrameNumber != 0 && RenderFrame.FrameNumber != LastSampledRenderFrameNumber)
	{
		Sample(RenderFrame);
		LastSampledRenderFrameNumber = RenderFrame.FrameNumber;
	}
	if (!bPaused && (LastSampledGameFrameNumber != 0 || LastSampledRenderFrameNumber != 0) &&
		(DisplayedGameFrame.FrameNumber == 0 || RefreshTimer <= 0.0f))
	{
		Refresh(GameFrame, RenderFrame);
		bRefreshed = true;
	}
	const FGPUFrameStatistics LastGPUStatistics = RenderSystem.GetLastGPUFrameStatistics();
	if (!bPaused && LastGPUStatistics.bValid && LastGPUStatistics.FrameNumber != LastSampledGPUFrameNumber)
	{
		LastSampledGPUFrameNumber = LastGPUStatistics.FrameNumber;
		if (DisplayedGPUStats.FrameNumber == 0 || RefreshTimer <= 0.0f || bRefreshed)
		{
			DisplayedGPUStats = LastGPUStatistics;
			if (!bRefreshed)
			{
				RefreshTimer = RefreshInterval;
			}
		}
	}
	const FInstancedDrawStatistics LastInstancedDrawStatistics = RenderSystem.GetLastInstancedDrawStatistics();
	if (!bPaused && LastInstancedDrawStatistics.bValid &&
		LastInstancedDrawStatistics.FrameNumber != LastSampledInstancedDrawFrameNumber)
	{
		LastSampledInstancedDrawFrameNumber = LastInstancedDrawStatistics.FrameNumber;
		if (DisplayedInstancedDrawStats.FrameNumber == 0 || RefreshTimer <= 0.0f || bRefreshed)
		{
			DisplayedInstancedDrawStats = LastInstancedDrawStatistics;
			if (!bRefreshed)
			{
				RefreshTimer = RefreshInterval;
			}
		}
	}

	if (!ImGui::Begin("Profile", nullptr, ImGuiWindowFlags_HorizontalScrollbar))
	{
		ImGui::End();
		return;
	}

	if (ImGui::Button(bPaused ? "Resume" : "Pause"))
	{
		bPaused = !bPaused;
		RefreshTimer = 0.0f;
	}
	ImGui::SameLine();
	ImGui::TextDisabled(bPaused ? "CPU/GPU/Draw sampling paused" : "CPU/GPU/Draw sampling active");
	ImGui::Separator();

	if (DisplayedGameFrame.FrameNumber == 0 && DisplayedRenderFrame.FrameNumber == 0 && !DisplayedGPUStats.bValid &&
		!DisplayedInstancedDrawStats.bValid)
	{
		ImGui::TextDisabled("Waiting for CPU/GPU profile data...");
		ImGui::End();
		return;
	}

	DrawGPUStats();
	const bool bHasGameStats = DisplayedGameFrame.FrameNumber != 0;
	const bool bHasRenderStats = DisplayedRenderFrame.FrameNumber != 0;
	if (bHasGameStats)
	{
		DrawCPUStats(ECPUProfileThread::Game, "CPU Stats (Game Thread)", "GameThreadCPUProfileStats");
	}
	if (bHasRenderStats)
	{
		DrawCPUStats(ECPUProfileThread::Render, "CPU Stats (Render Thread)", "RenderThreadCPUProfileStats");
	}
	DrawInstancedDrawStats();
	ImGui::End();
}

// 가장 최근에 완료된 비동기 GPU 프레임 시간과 Pipeline 호출 수를 표시한다.
void FProfilePanel::DrawGPUStats() const
{
	if (!ImGui::CollapsingHeader("GPU Stats", ImGuiTreeNodeFlags_DefaultOpen))
	{
		return;
	}
	if (!DisplayedGPUStats.bValid)
	{
		ImGui::TextDisabled("Waiting for GPU query data...");
		return;
	}

	constexpr ImGuiTableFlags TableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame
		| ImGuiTableFlags_NoSavedSettings;
	const float TableWidth = std::max(800.0f, ImGui::GetContentRegionAvail().x);
	if (ImGui::BeginTable("GPUProfileStats", 5, TableFlags, ImVec2(TableWidth, 0.0f)))
	{
		ImGui::TableSetupColumn("GPU Frame Time");
		ImGui::TableSetupColumn("IA Vertices");
		ImGui::TableSetupColumn("IA Primitives");
		ImGui::TableSetupColumn("VS Invocations");
		ImGui::TableSetupColumn("PS Invocations");
		ImGui::TableHeadersRow();
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("%.3f ms", DisplayedGPUStats.GPUTimeMs);
		ImGui::TableSetColumnIndex(1);
		ImGui::Text("%llu", static_cast<unsigned long long>(DisplayedGPUStats.IAVertices));
		ImGui::TableSetColumnIndex(2);
		ImGui::Text("%llu", static_cast<unsigned long long>(DisplayedGPUStats.IAPrimitives));
		ImGui::TableSetColumnIndex(3);
		ImGui::Text("%llu", static_cast<unsigned long long>(DisplayedGPUStats.VSInvocations));
		ImGui::TableSetColumnIndex(4);
		ImGui::Text("%llu", static_cast<unsigned long long>(DisplayedGPUStats.PSInvocations));
		ImGui::EndTable();
	}
}

// Profile ID를 배열 인덱스로 사용하여 완료 프레임 값을 누적 통계에 반영한다.
void FProfilePanel::Sample(const FCPUProfileFrame& Frame)
{
	for (const FCPUProfile& FrameProfile : Frame.Profiles)
	{
		const SIZE_T HistoryIndex = static_cast<SIZE_T>(FrameProfile.ProfileId) * 2 + static_cast<SIZE_T>(Frame.Thread);
		if (HistoryStats.size() <= HistoryIndex)
		{
			HistoryStats.resize(HistoryIndex + 1);
		}

		FCPUHistoryStat& HistoryStat = HistoryStats[HistoryIndex];
		if (HistoryStat.SampleCount == 0)
		{
			HistoryStat.Thread = Frame.Thread;
			HistoryStat.Category = FrameProfile.Category;
			HistoryStat.Name = FrameProfile.Name;
			HistoryStat.MinTimeMs = FrameProfile.InclusiveTimeMs;
		}

		HistoryStat.CallCount = FrameProfile.CallCount;
		HistoryStat.TotalTimeMs = FrameProfile.InclusiveTimeMs;
		HistoryStat.AccumulatedTimeMs += FrameProfile.InclusiveTimeMs;
		HistoryStat.MaxTimeMs = std::max(HistoryStat.MaxTimeMs, FrameProfile.InclusiveTimeMs);
		HistoryStat.MinTimeMs = std::min(HistoryStat.MinTimeMs, FrameProfile.InclusiveTimeMs);
		HistoryStat.AverageTimeMs = HistoryStat.AccumulatedTimeMs / static_cast<double>(++HistoryStat.SampleCount);
	}
}

// 관측된 Scope 통계를 표시용 Snapshot으로 갱신하고 정렬한다.
void FProfilePanel::Refresh(const FCPUProfileFrame& GameFrame, const FCPUProfileFrame& RenderFrame)
{
	DisplayedGameFrame = GameFrame;
	DisplayedRenderFrame = RenderFrame;

	DisplayedStats.clear();
	for (FCPUHistoryStat& HistoryStat : HistoryStats)
	{
		if (HistoryStat.SampleCount > 0)
		{
			DisplayedStats.push_back(HistoryStat);
			HistoryStat = {};
		}
	}
	std::stable_sort(DisplayedStats.begin(), DisplayedStats.end(), [](const FCPUHistoryStat& Left, const FCPUHistoryStat& Right)
	{
		if (Left.Thread != Right.Thread)
		{
			return Left.Thread < Right.Thread;
		}
		return Left.Category != Right.Category ? Left.Category < Right.Category : Left.TotalTimeMs > Right.TotalTimeMs;
	});
	RefreshTimer = RefreshInterval;
}

// 지정한 Thread의 CPU Scope 통계를 고정 열로 구성한다.
void FProfilePanel::DrawCPUStats(ECPUProfileThread Thread, const char* HeaderName, const char* TableName) const
{
	if (!ImGui::CollapsingHeader(HeaderName, ImGuiTreeNodeFlags_DefaultOpen))
	{
		return;
	}

	constexpr ImGuiTableFlags TableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable
		| ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings;
	const float TableWidth = std::max(800.0f, ImGui::GetContentRegionAvail().x);
	if (ImGui::BeginTable(TableName, 7, TableFlags, ImVec2(TableWidth, 0.0f)))
	{
		ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 100.0f);
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 260.0f);
		ImGui::TableSetupColumn("Calls", ImGuiTableColumnFlags_WidthFixed, 55.0f);
		ImGui::TableSetupColumn("Total(ms)", ImGuiTableColumnFlags_WidthFixed, 75.0f);
		ImGui::TableSetupColumn("Avg(ms)", ImGuiTableColumnFlags_WidthFixed, 75.0f);
		ImGui::TableSetupColumn("Max(ms)", ImGuiTableColumnFlags_WidthFixed, 75.0f);
		ImGui::TableSetupColumn("Min(ms)", ImGuiTableColumnFlags_WidthFixed, 75.0f);
		ImGui::TableHeadersRow();

		for (const FCPUHistoryStat& Stat : DisplayedStats)
		{
			if (Stat.Thread != Thread)
			{
				continue;
			}

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted(Stat.Category.c_str());
			ImGui::TableSetColumnIndex(1);
			ImGui::TextUnformatted(Stat.Name.c_str());
			ImGui::TableSetColumnIndex(2);
			ImGui::Text("%u", Stat.CallCount);
			ImGui::TableSetColumnIndex(3);
			ImGui::Text("%.3f", Stat.TotalTimeMs);
			ImGui::TableSetColumnIndex(4);
			ImGui::Text("%.3f", Stat.AverageTimeMs);
			ImGui::TableSetColumnIndex(5);
			ImGui::Text("%.3f", Stat.MaxTimeMs);
			ImGui::TableSetColumnIndex(6);
			ImGui::Text("%.3f", Stat.MinTimeMs);
		}
		ImGui::EndTable();
	}
}

// Opaque Pass의 자동 인스턴싱 적용량과 준비 비용을 Profile Panel의 마지막 카테고리에 표시한다.
void FProfilePanel::DrawInstancedDrawStats() const
{
	if (!ImGui::CollapsingHeader("Instanced Draws", ImGuiTreeNodeFlags_DefaultOpen))
	{
		return;
	}
	if (!DisplayedInstancedDrawStats.bValid)
	{
		ImGui::TextDisabled("Waiting for instanced draw data...");
		return;
	}

	constexpr ImGuiTableFlags TableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame
		| ImGuiTableFlags_NoSavedSettings;
	const float TableWidth = std::max(1120.0f, ImGui::GetContentRegionAvail().x);
	if (ImGui::BeginTable("InstancedDrawStats", 7, TableFlags, ImVec2(TableWidth, 0.0f)))
	{
		ImGui::TableSetupColumn("Visible Primitives");
		ImGui::TableSetupColumn("Instanced Primitives");
		ImGui::TableSetupColumn("Instance Batches");
		ImGui::TableSetupColumn("Instanced Draw Calls");
		ImGui::TableSetupColumn("Fallback Draw Calls");
		ImGui::TableSetupColumn("Batch Build Time");
		ImGui::TableSetupColumn("Instance Upload Time");
		ImGui::TableHeadersRow();
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("%llu", static_cast<unsigned long long>(DisplayedInstancedDrawStats.VisiblePrimitives));
		ImGui::TableSetColumnIndex(1);
		ImGui::Text("%llu", static_cast<unsigned long long>(DisplayedInstancedDrawStats.InstancedPrimitives));
		ImGui::TableSetColumnIndex(2);
		ImGui::Text("%llu", static_cast<unsigned long long>(DisplayedInstancedDrawStats.InstanceBatches));
		ImGui::TableSetColumnIndex(3);
		ImGui::Text("%llu", static_cast<unsigned long long>(DisplayedInstancedDrawStats.InstancedDrawCalls));
		ImGui::TableSetColumnIndex(4);
		ImGui::Text("%llu", static_cast<unsigned long long>(DisplayedInstancedDrawStats.FallbackDrawCalls));
		ImGui::TableSetColumnIndex(5);
		ImGui::Text("%.3f ms", DisplayedInstancedDrawStats.BatchBuildTimeMs);
		ImGui::TableSetColumnIndex(6);
		ImGui::Text("%.3f ms", DisplayedInstancedDrawStats.InstanceUploadTimeMs);
		ImGui::EndTable();
	}
}
