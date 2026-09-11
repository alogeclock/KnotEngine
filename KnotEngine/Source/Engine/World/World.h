#pragma once

#include "Core/CoreTypes.h"
#include "Object/Object.h"
#include "World/Level.h"
#include "Render/Scene/Scene.h"

class URenderer;

UENUM()
enum class EPlayState : uint8
{
	Playing,
	Paused,
	Stopped
};

// 하나의 독립적인 게임 시뮬레이션을 나타내는 최상위 객체.
// 소속 Level의 수명과 플레이 상태, 렌더 Scene을 관리한다.
// PersistentLevel은 Levels 배열에 포함되는 기본 Level이며 World 생성 시 함께 만들어진다.
UCLASS()
class ENGINE_API UWorld : public UObject
{
	GENERATED_CLASS(UWorld, UObject)

public:
	UWorld();
	~UWorld() override;

	ULevel& CreateLevel();
	ULevel& GetPersistentLevel() const;
	void RemoveLevel(ULevel& Level);

	void BeginPlay();
	void PausePlay();
	void ResumePlay();
	void EndPlay();

	void Tick(float DeltaTime);
	FScene& GetScene() { return Scene; }
	const FScene& GetScene() const { return Scene; }

	EPlayState GetPlayState() const { return PlayState; }
	const TArray<TObjectPtr<ULevel>>& GetLevels() const { return Levels; }

private:
	FScene Scene;

	UPROPERTY(NoEdit, Transient) TArray<TObjectPtr<ULevel>> Levels;
	UPROPERTY(NoEdit, Transient) TObjectPtr<ULevel> PersistentLevel;

	UPROPERTY(NoEdit, Transient) EPlayState PlayState = EPlayState::Stopped;
};
