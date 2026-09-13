#include "Core/Profiling/CPUProfiler.h"

#include "Core/Assert.h"

#include <algorithm>
#include <chrono>
#include <limits>

// 함수 지역 static으로 유지되는 메인 스레드 전용 수집 상태다.
struct FCPUProfiler::FState
{
	using FClock = std::chrono::steady_clock;

	// 아직 종료되지 않은 중첩 Scope의 시작 시각과 누적 자식 시간을 보관하는 스택 항목이다.
	struct FActiveScope
	{
		FCPUProfileId ProfileId = 0;
		FClock::time_point StartTime;
		double ChildTimeMs = 0.0;
	};

	TArray<FActiveScope> ScopeStack;
	TArray<FCPUProfile> Profiles;
	TMap<FString, TMap<FString, FCPUProfileId>> ProfileIds;

	FCPUProfileFrame LastFrame;
	FClock::time_point FrameStartTime = {};

	float FrameTimeMs = 0.0f;
	uint64 FrameNumber = 0;
	bool bFrameActive = false;
};

// 프로파일러가 사용하는 메인 스레드 전용 상태의 단일 인스턴스를 반환한다.
FCPUProfiler::FState& FCPUProfiler::GetState()
{
	static FState State;
	return State;
}

// Category와 Name 조합을 안정적인 Profile ID로 등록하거나 기존 ID를 반환한다.
FCPUProfileId FCPUProfiler::RegisterProfile(std::string_view Category, std::string_view Name)
{
	checkf(!Category.empty(), "CPU Profiler Category 이름이 비어 있다.");
	checkf(!Name.empty(), "CPU Profiler Scope 이름이 비어 있다.");

	FState& State = GetState();
	TMap<FString, FCPUProfileId>& CategoryProfiles = State.ProfileIds[FString(Category)];
	const auto ProfileIterator = CategoryProfiles.find(FString(Name));
	if (ProfileIterator != CategoryProfiles.end())
	{
		return ProfileIterator->second;
	}

	checkf(State.Profiles.size() < static_cast<SIZE_T>((std::numeric_limits<FCPUProfileId>::max)()), "CPU Profile 수가 FCPUProfileId 범위를 초과했다.");
	const FCPUProfileId ProfileId = static_cast<FCPUProfileId>(State.Profiles.size());
	FCPUProfile& Profile = State.Profiles.emplace_back();
	Profile.ProfileId = ProfileId;
	Profile.Category = Category;
	Profile.Name = Name;
	CategoryProfiles.emplace(Profile.Name, ProfileId);
	return ProfileId;
}

// 새 프레임의 시간 정보를 기록하고 등록된 모든 Profile 측정값을 초기화한다.
void FCPUProfiler::BeginFrame(float DeltaTime)
{
	FState& State = GetState();
	checkf(!State.bFrameActive && State.ScopeStack.empty(), "CPU Profiler Frame이 이미 시작되었다.");
	for (FCPUProfile& Profile : State.Profiles)
	{
		Profile.InclusiveTimeMs = 0.0;
		Profile.ExclusiveTimeMs = 0.0;
		Profile.MaxTimeMs = 0.0;
		Profile.CallCount = 0;
	}
	State.FrameTimeMs = DeltaTime * 1000.0f;
	State.FrameStartTime = FState::FClock::now();
	State.bFrameActive = true;
}

// 호출된 Profile만 프레임 Snapshot으로 복사하고 전체 CPU 측정 시간을 확정한다.
void FCPUProfiler::EndFrame()
{
	FState& State = GetState();
	checkf(State.bFrameActive && State.ScopeStack.empty(), "CPU Profiler Scope가 종료되기 전에 Frame을 종료할 수 없다.");
	const FState::FClock::time_point EndTime = FState::FClock::now();
	State.bFrameActive = false;

	State.LastFrame.Profiles.clear();
	State.LastFrame.Profiles.reserve(State.Profiles.size());
	for (const FCPUProfile& Profile : State.Profiles)
	{
		if (Profile.CallCount > 0)
		{
			State.LastFrame.Profiles.push_back(Profile);
		}
	}
	State.LastFrame.CPUTimeMs = std::chrono::duration<double, std::milli>(EndTime - State.FrameStartTime).count();
	State.LastFrame.FrameTimeMs = State.FrameTimeMs;
	State.LastFrame.FrameNumber = ++State.FrameNumber;
}

// 가장 최근에 완료된 CPU Profile Frame Snapshot을 반환한다.
const FCPUProfileFrame& FCPUProfiler::GetLastFrame()
{
	return GetState().LastFrame;
}

// 등록된 Profile ID의 Scope를 중첩 스택에 추가하고 시작 시각을 기록한다.
bool FCPUProfiler::BeginScope(FCPUProfileId ProfileId)
{
	FState& State = GetState();
	if (!State.bFrameActive)
	{
		return false;
	}

	checkf(ProfileId < State.Profiles.size(), "등록되지 않은 CPU Profile ID가 전달되었다.");
	State.ScopeStack.push_back({ ProfileId, FState::FClock::now(), 0.0 });
	return true;
}

// 마지막 Scope의 Inclusive와 Exclusive 시간을 ID로 지정된 프레임 통계에 누적한다.
void FCPUProfiler::EndScope()
{
	FState& State = GetState();
	checkf(State.bFrameActive && !State.ScopeStack.empty(), "시작되지 않은 CPU Profiler Scope를 종료할 수 없다.");
	const FState::FClock::time_point EndTime = FState::FClock::now();
	const FState::FActiveScope ActiveScope = State.ScopeStack.back();
	State.ScopeStack.pop_back();

	const double InclusiveTimeMs = std::chrono::duration<double, std::milli>(EndTime - ActiveScope.StartTime).count();
	const double ExclusiveTimeMs = InclusiveTimeMs - ActiveScope.ChildTimeMs;
	if (!State.ScopeStack.empty())
	{
		State.ScopeStack.back().ChildTimeMs += InclusiveTimeMs;
	}

	FCPUProfile& Profile = State.Profiles[ActiveScope.ProfileId];
	Profile.InclusiveTimeMs += InclusiveTimeMs;
	Profile.ExclusiveTimeMs += std::max(0.0, ExclusiveTimeMs);
	Profile.MaxTimeMs = std::max(Profile.MaxTimeMs, InclusiveTimeMs);
	++Profile.CallCount;
}

// 등록된 Profile ID의 CPU Scope를 시작한다.
FCPUProfilerScope::FCPUProfilerScope(FCPUProfileId ProfileId)
	: bActive(FCPUProfiler::BeginScope(ProfileId))
{
}

// 활성 Scope가 수명 범위를 벗어나면 측정을 종료하고 결과를 누적한다.
FCPUProfilerScope::~FCPUProfilerScope()
{
	if (bActive)
	{
		FCPUProfiler::EndScope();
	}
}
