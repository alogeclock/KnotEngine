#include "Editor/Panel/ProfilePanel.h"

#include "Render/RenderSystem.h"
#include "Runtime/EditorEngine.h"

#include <algorithm>
#include <imgui.h>

FProfilePanel::FProfilePanel(FRenderSystem& InRenderSystem)
	: RenderSystem(InRenderSystem)
{
}

// 완료된 Frame과 Editor 작업을 샘플링하고 선택한 통계 탭을 그린다.
void FProfilePanel::Draw(float DeltaTime, const FPickingStatistics& PickingStatistics)
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

	const FTransactionStatistics& TransactionStatistics = GetEditor().GetEditorTransaction().GetStatistics();
	const bool bFirstPicking = DisplayedPickingStats.QueryCount == 0 && PickingStatistics.QueryCount != 0;
	const bool bFirstTransaction = DisplayedTransactionStats.Undo.OperationCount + DisplayedTransactionStats.Redo.OperationCount == 0 &&
		TransactionStatistics.Undo.OperationCount + TransactionStatistics.Redo.OperationCount != 0;
	if (!bPaused && (RefreshTimer <= 0.0f || bRefreshed || bFirstPicking || bFirstTransaction))
	{
		DisplayedPickingStats = PickingStatistics;
		DisplayedTransactionStats = TransactionStatistics;
		if (!bRefreshed)
		{
			RefreshTimer = RefreshInterval;
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
	ImGui::TextDisabled(bPaused ? "Profile display paused" : "Profile display active");
	ImGui::Separator();

	if (ImGui::BeginTabBar("ProfileTabs"))
	{
		if (ImGui::BeginTabItem("Frame"))
		{
			if (DisplayedGameFrame.FrameNumber == 0 && DisplayedRenderFrame.FrameNumber == 0 && !DisplayedGPUStats.bValid &&
				!DisplayedInstancedDrawStats.bValid)
			{
				ImGui::TextDisabled("Waiting for CPU/GPU profile data...");
			}
			else
			{
				DrawGPUStats();
				if (DisplayedGameFrame.FrameNumber != 0)
				{
					DrawCPUStats(ECPUProfileThread::Game, "CPU Stats (Game Thread)", "GameThreadCPUProfileStats");
				}
				if (DisplayedRenderFrame.FrameNumber != 0)
				{
					DrawCPUStats(ECPUProfileThread::Render, "CPU Stats (Render Thread)", "RenderThreadCPUProfileStats");
				}
				DrawInstancedDrawStats();
			}
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Editor"))
		{
			DrawPickingStats();
			DrawTransactionStats();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
	ImGui::End();
}

// 마지막 Picking의 시간과 검사량을 실제 질의 기준의 누적 통계와 함께 표시한다.
void FProfilePanel::DrawPickingStats() const
{
	if (!ImGui::CollapsingHeader("Picking Stats", ImGuiTreeNodeFlags_DefaultOpen))
	{
		return;
	}
	if (DisplayedPickingStats.QueryCount == 0)
	{
		ImGui::TextUnformatted("Queries: - | Result: -");
	}
	else
	{
		ImGui::Text("Queries: %llu | Result: %s", static_cast<unsigned long long>(DisplayedPickingStats.QueryCount),
			DisplayedPickingStats.bHit ? "Hit" : "Miss");
	}
	ImGui::TextDisabled("Last CPU raycast (LOD 0). Average and maximum cover all completed queries.");
	constexpr ImGuiTableFlags TableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame
		| ImGuiTableFlags_NoSavedSettings;
	const float TableWidth = std::max(1100.0f, ImGui::GetContentRegionAvail().x);
	if (ImGui::BeginTable("PickingStats", 8, TableFlags, ImVec2(TableWidth, 0.0f)))
	{
		ImGui::TableSetupColumn("Last Time");
		ImGui::TableSetupColumn("Scene Query");
		ImGui::TableSetupColumn("Mesh Query");
		ImGui::TableSetupColumn("Average Time");
		ImGui::TableSetupColumn("Max Time");
		ImGui::TableSetupColumn("Bounds Tests");
		ImGui::TableSetupColumn("Candidate Meshes");
		ImGui::TableSetupColumn("Triangle Tests");
		ImGui::TableHeadersRow();
		ImGui::TableNextRow();
		if (DisplayedPickingStats.QueryCount == 0)
		{
			for (int32 Column = 0; Column < 8; ++Column)
			{
				ImGui::TableSetColumnIndex(Column);
				ImGui::TextDisabled("-");
			}
			ImGui::EndTable();
			return;
		}
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("%.3f ms", DisplayedPickingStats.TotalTimeMs);
		ImGui::TableSetColumnIndex(1);
		ImGui::Text("%.3f ms", DisplayedPickingStats.SceneQueryTimeMs);
		ImGui::TableSetColumnIndex(2);
		ImGui::Text("%.3f ms", DisplayedPickingStats.MeshQueryTimeMs);
		ImGui::TableSetColumnIndex(3);
		ImGui::Text("%.3f ms", DisplayedPickingStats.AccumulatedTimeMs / static_cast<double>(DisplayedPickingStats.QueryCount));
		ImGui::TableSetColumnIndex(4);
		ImGui::Text("%.3f ms", DisplayedPickingStats.MaxTimeMs);
		ImGui::TableSetColumnIndex(5);
		ImGui::Text("%llu", static_cast<unsigned long long>(DisplayedPickingStats.BoundsTests));
		ImGui::TableSetColumnIndex(6);
		ImGui::Text("%llu", static_cast<unsigned long long>(DisplayedPickingStats.CandidateMeshes));
		ImGui::TableSetColumnIndex(7);
		ImGui::Text("%llu", static_cast<unsigned long long>(DisplayedPickingStats.TriangleTests));
		ImGui::EndTable();
	}
}

// Undo와 Redo의 마지막 작업 이름 및 적용량과 누적 실행 시간을 각각 표시한다.
void FProfilePanel::DrawTransactionStats() const
{
	if (!ImGui::CollapsingHeader("Transaction Stats", ImGuiTreeNodeFlags_DefaultOpen))
	{
		return;
	}
	ImGui::TextDisabled("Completed Undo/Redo only. Records and description refer to the last operation in each row.");
	constexpr ImGuiTableFlags TableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame
		| ImGuiTableFlags_NoSavedSettings;
	const float TableWidth = std::max(1000.0f, ImGui::GetContentRegionAvail().x);
	if (ImGui::BeginTable("TransactionStats", 7, TableFlags, ImVec2(TableWidth, 0.0f)))
	{
		ImGui::TableSetupColumn("Operation");
		ImGui::TableSetupColumn("Calls");
		ImGui::TableSetupColumn("Records");
		ImGui::TableSetupColumn("Last Time");
		ImGui::TableSetupColumn("Average Time");
		ImGui::TableSetupColumn("Max Time");
		ImGui::TableSetupColumn("Description");
		ImGui::TableHeadersRow();
		const FTransactionOperationStatistics* Operations[] = { &DisplayedTransactionStats.Undo, &DisplayedTransactionStats.Redo };
		const char* Names[] = { "Undo", "Redo" };
		for (SIZE_T Index = 0; Index < 2; ++Index)
		{
			const FTransactionOperationStatistics& Stat = *Operations[Index];
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted(Names[Index]);
			if (Stat.OperationCount == 0)
			{
				for (int32 Column = 1; Column < 7; ++Column)
				{
					ImGui::TableSetColumnIndex(Column);
					ImGui::TextDisabled("-");
				}
				continue;
			}
			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%llu", static_cast<unsigned long long>(Stat.OperationCount));
			ImGui::TableSetColumnIndex(2);
			ImGui::Text("%llu", static_cast<unsigned long long>(Stat.RecordCount));
			ImGui::TableSetColumnIndex(3);
			ImGui::Text("%.3f ms", Stat.TotalTimeMs);
			ImGui::TableSetColumnIndex(4);
			ImGui::Text("%.3f ms", Stat.AccumulatedTimeMs / static_cast<double>(Stat.OperationCount));
			ImGui::TableSetColumnIndex(5);
			ImGui::Text("%.3f ms", Stat.MaxTimeMs);
			ImGui::TableSetColumnIndex(6);
			ImGui::TextUnformatted(Stat.Description.c_str());
		}
		ImGui::EndTable();
	}
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
