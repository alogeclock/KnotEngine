#include "Editor/Widget/ViewportOverlayWidget.h"

#include "Core/Memory/Memory.h"

#include <imgui.h>

// 공유 표시 상태를 참조하도록 Viewport 통계 오버레이를 구성한다.
FViewportOverlayWidget::FViewportOverlayWidget(const FViewportStatState& InState)
	: State(InState)
{
}

// 짧은 프레임 구간을 누적하여 안정적인 FPS와 평균 frame time을 갱신한다.
void FViewportOverlayWidget::Tick(float DeltaTime)
{
	if (DeltaTime <= 0.0f)
	{
		return;
	}

	SampleTime += DeltaTime;
	++SampleCount;
	static constexpr float RefreshInterval = 0.25f;
	if (SampleTime >= RefreshInterval)
	{
		DisplayedFPS = static_cast<float>(SampleCount) / SampleTime;
		DisplayedFrameTimeMs = SampleTime / static_cast<float>(SampleCount) * 1000.0f;
		SampleTime = 0.0f;
		SampleCount = 0;
	}
}

// 직전에 그린 Viewport 이미지의 왼쪽 위에 활성화된 통계를 반투명 창으로 표시한다.
void FViewportOverlayWidget::Draw() const
{
	if (!State.bShowFPS && !State.bShowMemory)
	{
		return;
	}

	const ImVec2 ImagePosition = ImGui::GetItemRectMin();
	const ImVec2 OverlayPosition(ImagePosition.x + 8.0f, ImagePosition.y + 8.0f);
	const ImGuiID PlatformViewportId = ImGui::GetWindowViewport()->ID;
	ImGui::SetNextWindowPos(OverlayPosition, ImGuiCond_Always);
	ImGui::SetNextWindowViewport(PlatformViewportId);
	ImGui::SetNextWindowBgAlpha(0.45f);
	constexpr ImGuiWindowFlags OverlayFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize
		| ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav
		| ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));
	if (ImGui::Begin("##ViewportStatisticsOverlay", nullptr, OverlayFlags))
	{
		if (State.bShowFPS)
		{
			ImGui::Text("FPS: %.1f (%.3f ms)", DisplayedFPS, DisplayedFrameTimeMs);
		}
		if (State.bShowMemory)
		{
			const uint64 AllocatedCount = TotalAllocatedCount.load(std::memory_order_relaxed);
			const uint64 AllocatedBytes = TotalAllocatedBytes.load(std::memory_order_relaxed);
			ImGui::Text("Total Allocated Count: %llu", static_cast<unsigned long long>(AllocatedCount));
			ImGui::Text("Total Allocated Bytes: %llu", static_cast<unsigned long long>(AllocatedBytes));
		}
	}
	ImGui::End();
	ImGui::PopStyleVar();
}
