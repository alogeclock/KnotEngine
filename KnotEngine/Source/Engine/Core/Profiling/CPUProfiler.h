#pragma once

#include "EngineAPI.h"

#include "Core/CoreTypes.h"

#include <string_view>

#define KNOT_CPU_PROFILER_ENABLED 1

// 등록된 Category와 Name 조합을 배열에서 직접 찾기 위한 CPU Profile ID다.
using FCPUProfileId = uint32;

enum class ECPUProfileThread : uint8
{
	Game,
	Render,
};

// 완료된 한 프레임에서 동일한 Category와 Name으로 실행된 CPU Scope의 합산 결과다.
struct ENGINE_API FCPUProfile
{
	FCPUProfileId ProfileId = 0;
	FString Category;
	FString Name;
	double InclusiveTimeMs = 0.0;
	double ExclusiveTimeMs = 0.0;
	double MaxTimeMs = 0.0;
	uint32 CallCount = 0;
};

// 한 프레임의 Scope 통계와 전체 CPU 측정 구간 및 실제 프레임 시간을 보관하는 완료 Snapshot이다.
struct ENGINE_API FCPUProfileFrame
{
	TArray<FCPUProfile> Profiles;
	double CPUTimeMs = 0.0;
	float FrameTimeMs = 0.0f;
	uint64 FrameNumber = 0;
	ECPUProfileThread Thread = ECPUProfileThread::Game;
};

// 메인 스레드에서 한 프레임의 CPU Scope를 수집하고 다음 프레임이 끝날 때까지 유효한 완료 Snapshot을 제공한다.
class ENGINE_API FCPUProfiler final
{
public:
	static FCPUProfileId RegisterProfile(std::string_view Category, std::string_view Name);
	static void BeginFrame(ECPUProfileThread Thread, float DeltaTime);
	static void EndFrame();
	static FCPUProfileFrame GetLastFrame(ECPUProfileThread Thread);

private:
	friend class FCPUProfilerScope;

	struct FState;
	struct FGlobalState;
	static FState& GetThreadState();
	static FGlobalState& GetGlobalState();
	static bool BeginScope(FCPUProfileId ProfileId);
	static void EndScope();
};

// 생성과 소멸 사이의 시간을 측정하고 중첩된 자식 시간을 제외한 Exclusive 시간까지 기록하는 RAII Scope다.
class ENGINE_API FCPUProfilerScope final
{
public:
	explicit FCPUProfilerScope(FCPUProfileId ProfileId);
	~FCPUProfilerScope();

	FCPUProfilerScope(const FCPUProfilerScope&) = delete;
	FCPUProfilerScope& operator=(const FCPUProfilerScope&) = delete;

private:
	bool bActive = false;
};

#if KNOT_CPU_PROFILER_ENABLED
#define KNOT_PROFILE_JOIN_INNER(Left, Right) Left##Right
#define KNOT_PROFILE_JOIN(Left, Right) KNOT_PROFILE_JOIN_INNER(Left, Right)
#define KNOT_PROFILE_SCOPE(Category, Name) \
	static const FCPUProfileId KNOT_PROFILE_JOIN(CPUProfileId_, __LINE__) = FCPUProfiler::RegisterProfile(Category, Name); \
	FCPUProfilerScope KNOT_PROFILE_JOIN(CPUProfilerScope_, __LINE__)(KNOT_PROFILE_JOIN(CPUProfileId_, __LINE__))
#define KNOT_PROFILE_FUNCTION(Category) KNOT_PROFILE_SCOPE(Category, __FUNCTION__)
#else
#define KNOT_PROFILE_SCOPE(Category, Name) do { } while (false)
#define KNOT_PROFILE_FUNCTION(Category) do { } while (false)
#endif
