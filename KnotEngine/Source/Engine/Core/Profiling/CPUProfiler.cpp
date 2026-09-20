#include "Core/Profiling/CPUProfiler.h"

#include "Core/Assert.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <limits>
#include <mutex>

struct FCPUProfiler::FState
{
	using FClock = std::chrono::steady_clock;

	struct FActiveScope
	{
		FCPUProfileId ProfileId = 0;
		FClock::time_point StartTime;
		double ChildTimeMs = 0.0;
	};

	TArray<FActiveScope> ScopeStack;
	TArray<FCPUProfile> Profiles;
	FClock::time_point FrameStartTime = {};
	ECPUProfileThread Thread = ECPUProfileThread::Game;
	float FrameTimeMs = 0.0f;
	uint64 FrameNumber = 0;
	bool bFrameActive = false;
};

struct FCPUProfiler::FGlobalState
{
	struct FProfileDefinition
	{
		FString Category;
		FString Name;
	};

	std::mutex RegistryMutex;
	TMap<FString, TMap<FString, FCPUProfileId>> ProfileIds;
	TArray<FProfileDefinition> ProfileDefinitions;

	std::mutex SnapshotMutex;
	std::array<FCPUProfileFrame, 2> LastFrames;
};

FCPUProfiler::FState& FCPUProfiler::GetThreadState()
{
	thread_local FState State;
	return State;
}

FCPUProfiler::FGlobalState& FCPUProfiler::GetGlobalState()
{
	static FGlobalState State;
	return State;
}

FCPUProfileId FCPUProfiler::RegisterProfile(std::string_view Category, std::string_view Name)
{
	checkf(!Category.empty(), "CPU Profiler Category 이름이 비어 있다.");
	checkf(!Name.empty(), "CPU Profiler Scope 이름이 비어 있다.");

	FGlobalState& State = GetGlobalState();
	std::lock_guard Lock(State.RegistryMutex);
	TMap<FString, FCPUProfileId>& CategoryProfiles = State.ProfileIds[FString(Category)];
	const auto ProfileIterator = CategoryProfiles.find(FString(Name));
	if (ProfileIterator != CategoryProfiles.end())
	{
		return ProfileIterator->second;
	}

	checkf(State.ProfileDefinitions.size() < static_cast<SIZE_T>((std::numeric_limits<FCPUProfileId>::max)()),
	       "CPU Profile 수가 FCPUProfileId 범위를 초과했다.");
	const FCPUProfileId ProfileId = static_cast<FCPUProfileId>(State.ProfileDefinitions.size());
	State.ProfileDefinitions.push_back({ FString(Category), FString(Name) });
	CategoryProfiles.emplace(State.ProfileDefinitions.back().Name, ProfileId);
	return ProfileId;
}

void FCPUProfiler::BeginFrame(ECPUProfileThread Thread, float DeltaTime)
{
	FState& State = GetThreadState();
	checkf(!State.bFrameActive && State.ScopeStack.empty(), "CPU Profiler Frame이 이미 시작되었다.");
	for (FCPUProfile& Profile : State.Profiles)
	{
		Profile.InclusiveTimeMs = 0.0;
		Profile.ExclusiveTimeMs = 0.0;
		Profile.MaxTimeMs = 0.0;
		Profile.CallCount = 0;
	}
	State.Thread = Thread;
	State.FrameTimeMs = DeltaTime * 1000.0f;
	State.FrameStartTime = FState::FClock::now();
	State.bFrameActive = true;
}

void FCPUProfiler::EndFrame()
{
	FState& State = GetThreadState();
	checkf(State.bFrameActive && State.ScopeStack.empty(), "CPU Profiler Scope가 종료되기 전에 Frame을 종료할 수 없다.");
	const FState::FClock::time_point EndTime = FState::FClock::now();
	State.bFrameActive = false;

	FCPUProfileFrame Frame;
	Frame.Profiles.reserve(State.Profiles.size());
	for (const FCPUProfile& Profile : State.Profiles)
	{
		if (Profile.CallCount > 0)
		{
			Frame.Profiles.push_back(Profile);
		}
	}
	Frame.CPUTimeMs = std::chrono::duration<double, std::milli>(EndTime - State.FrameStartTime).count();
	Frame.FrameTimeMs = State.FrameTimeMs;
	Frame.FrameNumber = ++State.FrameNumber;
	Frame.Thread = State.Thread;

	FGlobalState& GlobalState = GetGlobalState();
	std::lock_guard Lock(GlobalState.SnapshotMutex);
	GlobalState.LastFrames[static_cast<SIZE_T>(State.Thread)] = std::move(Frame);
}

FCPUProfileFrame FCPUProfiler::GetLastFrame(ECPUProfileThread Thread)
{
	FGlobalState& State = GetGlobalState();
	std::lock_guard Lock(State.SnapshotMutex);
	return State.LastFrames[static_cast<SIZE_T>(Thread)];
}

bool FCPUProfiler::BeginScope(FCPUProfileId ProfileId)
{
	FState& State = GetThreadState();
	if (!State.bFrameActive)
	{
		return false;
	}

	if (State.Profiles.size() <= ProfileId)
	{
		FGlobalState& GlobalState = GetGlobalState();
		std::lock_guard Lock(GlobalState.RegistryMutex);
		checkf(ProfileId < GlobalState.ProfileDefinitions.size(), "등록되지 않은 CPU Profile ID가 전달되었다.");
		const SIZE_T PreviousSize = State.Profiles.size();
		State.Profiles.resize(static_cast<SIZE_T>(ProfileId) + 1);
		for (SIZE_T Index = PreviousSize; Index < State.Profiles.size(); ++Index)
		{
			const FGlobalState::FProfileDefinition& Definition = GlobalState.ProfileDefinitions[Index];
			State.Profiles[Index].ProfileId = static_cast<FCPUProfileId>(Index);
			State.Profiles[Index].Category = Definition.Category;
			State.Profiles[Index].Name = Definition.Name;
		}
	}

	State.ScopeStack.push_back({ ProfileId, FState::FClock::now(), 0.0 });
	return true;
}

void FCPUProfiler::EndScope()
{
	FState& State = GetThreadState();
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

FCPUProfilerScope::FCPUProfilerScope(FCPUProfileId ProfileId)
	: bActive(FCPUProfiler::BeginScope(ProfileId))
{
}

FCPUProfilerScope::~FCPUProfilerScope()
{
	if (bActive)
	{
		FCPUProfiler::EndScope();
	}
}
