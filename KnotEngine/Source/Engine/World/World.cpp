#include "World/World.h"

#include "Core/Profiling/CPUProfiler.h"

UWorld::UWorld()
{
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
	ULevel* Level = GUObjectManager.Create<ULevel>(*this);
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

// BaseName별로 증가하는 숫자 접미사를 붙여 새 Node 이름을 생성한다.
FName UWorld::GetNodeName(const FString& BaseName)
{
	uint64& Suffix = NameCounters[BaseName];
	const FString Name = Suffix == 0 ? BaseName : BaseName + " " + std::to_string(Suffix);
	++Suffix;
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
