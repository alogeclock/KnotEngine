#include "World/World.h"

#include "Core/Profiling/CPUProfiler.h"

#include <algorithm>
#include <charconv>
#include <limits>

UWorld::UWorld()
{
}

// CDO가 아닌 실제 World에만 런타임 Persistent Level을 생성한다.
void UWorld::PostInitProperties()
{
	Super::PostInitProperties();
	if (!IsTemplate())
	{
		check(Levels.empty() && !PersistentLevel);
		PersistentLevel = &CreateLevel();
	}
}

// 복제 시 제외된 런타임 Level 상태를 새 Persistent Level로 초기화한다.
void UWorld::PostDuplicate()
{
	Super::PostDuplicate();
	check(Levels.empty() && !PersistentLevel);
	PersistentLevel = &CreateLevel();
}

UWorld::~UWorld()
{
	EndPlay();
	for (const auto& Level : Levels)
	{
		GUObjectManager.Destroy(Level.Get());
	}
}

ULevel& UWorld::CreateLevel()
{
	ULevel* Level = NewObject<ULevel>(this);
	Levels.emplace_back(Level);
	return *Level;
}

ULevel& UWorld::GetPersistentLevel() const
{
	check(PersistentLevel);
	return *PersistentLevel;
}

void UWorld::RemoveLevel(ULevel& Level)
{
	panic(&Level != PersistentLevel.Get());
	for (SIZE_T Index = 0; Index < Levels.size(); ++Index)
	{
		if (Levels[Index].Get() == &Level)
		{
			Level.EndPlay();
			Levels.erase(Levels.begin() + Index);
			GUObjectManager.Destroy(&Level);
			return;
		}
	}
	checkf(&Level.GetWorld() == this, "이 World에 속하지 않은 Level을 제거할 수 없다.");
}

// 정지된 World에서 Persistent Level은 유지하고 모든 Node와 추가 Level 및 이름 상태를 제거한다.
void UWorld::Reset()
{
	check(PlayState == EPlayState::Stopped);
	for (const TObjectPtr<ULevel>& Level : Levels)
	{
		while (!Level->GetNodes().empty())
		{
			Level->RemoveNode(*Level->GetNodes().back());
		}
	}
	while (Levels.size() > 1)
	{
		RemoveLevel(*Levels.back());
	}
	NameCounters.clear();
}

// 검증된 임시 World의 평탄 Level/Node 상태를 현재 World에 커밋한다.
void UWorld::Replace(UWorld& LoadedWorld)
{
	check(this != &LoadedWorld && PlayState == EPlayState::Stopped && LoadedWorld.PlayState == EPlayState::Stopped);

	Reset();

	for (SIZE_T LevelIndex = 0; LevelIndex < LoadedWorld.Levels.size(); ++LevelIndex)
	{
		ULevel& Destination = LevelIndex == 0 ? GetPersistentLevel() : CreateLevel();
		LoadedWorld.Levels[LevelIndex]->TransferNodes(Destination);
	}
	NameCounters = std::move(LoadedWorld.NameCounters);
}

// 이름 끝의 숫자 접미사를 분리하고 같은 Base Name에서 사용할 다음 접미사를 계산한다.
void UWorld::ParseNodeName(const FString& Name, FString& OutBaseName, uint64& OutNextSuffix)
{
	OutBaseName = Name;
	OutNextSuffix = 1;

	const SIZE_T SuffixOffset = Name.find_last_of(' ');
	if (SuffixOffset == FString::npos || SuffixOffset + 1 >= Name.size())
	{
		return;
	}

	uint64 Suffix = 0;
	const char* First = Name.data() + SuffixOffset + 1;
	const auto Result = std::from_chars(First, Name.data() + Name.size(), Suffix);
	if (Result.ec == std::errc() && Result.ptr == Name.data() + Name.size() && Suffix < (std::numeric_limits<uint64>::max)())
	{
		OutBaseName = Name.substr(0, SuffixOffset);
		OutNextSuffix = Suffix + 1;
	}
}

// 명시적으로 생성되거나 편집된 이름을 Base Name별 다음 접미사 맵에 반영한다.
void UWorld::RegisterNodeName(const FString& Name)
{
	FString BaseName;
	uint64 NextSuffix = 0;
	ParseNodeName(Name, BaseName, NextSuffix);
	NameCounters[BaseName] = (std::max)(NameCounters[BaseName], NextSuffix);
}

// 이미 숫자 접미사가 있는 이름도 같은 Base Name 계열의 다음 고유 이름으로 발급한다.
FName UWorld::GetNodeName(const FString& BaseName)
{
	FString NormalizedBaseName;
	uint64 ParsedNextSuffix = 0;
	ParseNodeName(BaseName, NormalizedBaseName, ParsedNextSuffix);

	uint64& NextSuffix = NameCounters[NormalizedBaseName];
	const FString Name = NextSuffix == 0 ? NormalizedBaseName : NormalizedBaseName + " " + std::to_string(NextSuffix);
	++NextSuffix;
	return FName(Name);
}

void UWorld::BeginPlay()
{
	if (PlayState != EPlayState::Stopped)
	{
		return;
	}
	PlayState = EPlayState::Playing;
	for (const TObjectPtr<ULevel>& Level : Levels)
	{
		Level->BeginPlay();
	}
}

void UWorld::EndPlay()
{
	if (PlayState == EPlayState::Stopped)
	{
		return;
	}
	PlayState = EPlayState::Stopped;
	for (const TObjectPtr<ULevel>& Level : Levels)
	{
		Level->EndPlay();
	}
}

void UWorld::PausePlay()
{
	if (PlayState == EPlayState::Playing)
	{
		PlayState = EPlayState::Paused;
	}
}

void UWorld::ResumePlay()
{
	if (PlayState == EPlayState::Paused)
	{
		PlayState = EPlayState::Playing;
	}
}

void UWorld::Tick(float DeltaTime)
{
	KNOT_PROFILE_SCOPE("Tick", "UWorld::Tick");

	if (PlayState == EPlayState::Playing)
	{
		for (const TObjectPtr<ULevel>& Level : Levels)
		{
			Level->Tick(DeltaTime);
		}
	}
}
