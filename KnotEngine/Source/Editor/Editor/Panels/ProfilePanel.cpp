#include "Editor/Panels/ProfilePanel.h"

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
	const FCPUProfileFrame& LastFrame = FCPUProfiler::GetLastFrame();
	bool bRefreshed = false;
	if (!bPaused && LastFrame.FrameNumber != 0 && LastFrame.FrameNumber != LastSampledFrameNumber)
	{
		Sample(LastFrame);
		LastSampledFrameNumber = LastFrame.FrameNumber;
		if (DisplayedFrame.FrameNumber == 0 || RefreshTimer <= 0.0f)
		{
			Refresh(LastFrame);
			bRefreshed = true;
		}
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

	if (!ImGui::Begin("Profile"))
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
	ImGui::TextDisabled(bPaused ? "CPU/GPU sampling paused" : "CPU/GPU sampling active");
	ImGui::Separator();

	if (DisplayedFrame.FrameNumber == 0 && !DisplayedGPUStats.bValid)
	{
		ImGui::TextDisabled("Waiting for CPU/GPU profile data...");
		ImGui::End();
		return;
	}

	DrawGPUStats();
	if (DisplayedFrame.FrameNumber != 0)
	{
		DrawCPUStats();
	}
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

	if (ImGui::BeginTable("GPUProfileStats", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
	{
		ImGui::TableSetupColumn("Statistic");
		ImGui::TableSetupColumn("Value");
		ImGui::TableHeadersRow();
		const auto DrawRow = [](const char* Name, const char* Format, auto Value)
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted(Name);
			ImGui::TableSetColumnIndex(1);
			ImGui::Text(Format, Value);
		};
		DrawRow("GPU Frame Time", "%.3f ms", DisplayedGPUStats.GPUTimeMs);
		DrawRow("IA Vertices", "%llu", static_cast<unsigned long long>(DisplayedGPUStats.IAVertices));
		DrawRow("IA Primitives", "%llu", static_cast<unsigned long long>(DisplayedGPUStats.IAPrimitives));
		DrawRow("VS Invocations", "%llu", static_cast<unsigned long long>(DisplayedGPUStats.VSInvocations));
		DrawRow("PS Invocations", "%llu", static_cast<unsigned long long>(DisplayedGPUStats.PSInvocations));
		ImGui::EndTable();
	}
}

// Profile ID를 배열 인덱스로 사용하여 완료 프레임 값을 누적 통계에 반영한다.
void FProfilePanel::Sample(const FCPUProfileFrame& Frame)
{
	for (const FCPUProfile& FrameProfile : Frame.Profiles)
	{
		if (HistoryStats.size() <= FrameProfile.ProfileId)
		{
			HistoryStats.resize(static_cast<SIZE_T>(FrameProfile.ProfileId) + 1);
		}

		FCPUHistoryStat& HistoryStat = HistoryStats[FrameProfile.ProfileId];
		if (HistoryStat.SampleCount == 0)
		{
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
void FProfilePanel::Refresh(const FCPUProfileFrame& Frame)
{
	DisplayedFrame = Frame;

	DisplayedStats.clear();
	for (const FCPUHistoryStat& HistoryStat : HistoryStats)
	{
		if (HistoryStat.SampleCount > 0)
		{
			DisplayedStats.push_back(HistoryStat);
		}
	}
	std::stable_sort(DisplayedStats.begin(), DisplayedStats.end(), [](const FCPUHistoryStat& Left, const FCPUHistoryStat& Right)
	{
		return Left.Category != Right.Category ? Left.Category < Right.Category : Left.TotalTimeMs > Right.TotalTimeMs;
	});
	RefreshTimer = RefreshInterval;
}

// 표시용 CPU Scope 통계를 Category별 행과 고정 열로 구성한다.
void FProfilePanel::DrawCPUStats() const
{
	if (!ImGui::CollapsingHeader("CPU Stats", ImGuiTreeNodeFlags_DefaultOpen))
	{
		return;
	}

	constexpr ImGuiTableFlags TableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable
		| ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;
	const float TableHeight = std::max(1.0f, ImGui::GetContentRegionAvail().y);
	if (ImGui::BeginTable("CPUProfileStats", 7, TableFlags, ImVec2(0.0f, TableHeight)))
	{
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 100.0f);
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 180.0f);
		ImGui::TableSetupColumn("Calls", ImGuiTableColumnFlags_WidthFixed, 55.0f);
		ImGui::TableSetupColumn("Total(ms)", ImGuiTableColumnFlags_WidthFixed, 75.0f);
		ImGui::TableSetupColumn("Avg(ms)", ImGuiTableColumnFlags_WidthFixed, 75.0f);
		ImGui::TableSetupColumn("Max(ms)", ImGuiTableColumnFlags_WidthFixed, 75.0f);
		ImGui::TableSetupColumn("Min(ms)", ImGuiTableColumnFlags_WidthFixed, 75.0f);
		ImGui::TableHeadersRow();

		FString PreviousCategory;
		for (const FCPUHistoryStat& Stat : DisplayedStats)
		{
			if (Stat.Category != PreviousCategory)
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted(Stat.Category.c_str());
				for (int ColumnIndex = 1; ColumnIndex < 7; ++ColumnIndex)
				{
					ImGui::TableSetColumnIndex(ColumnIndex);
					ImGui::TextDisabled("---");
				}
				PreviousCategory = Stat.Category;
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
