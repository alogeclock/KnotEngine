#pragma once

#include "Core/CoreTypes.h"
#include "Object/Object.h"
#include "GameFramework/Level.h"

class URenderer;

UENUM()
enum class EPlayState : uint8
{
	Playing,
	Paused,
	Stopped
};

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
	void Render(URenderer& Renderer, const FMatrix& ViewProjection) const;

	EPlayState GetPlayState() const { return PlayState; }
	const TArray<TObjectPtr<ULevel>>& GetLevels() const { return Levels; }

private:
	UPROPERTY(NoEdit, Transient) TArray<TObjectPtr<ULevel>> Levels;
	UPROPERTY(NoEdit, Transient) TObjectPtr<ULevel> PersistentLevel;

	UPROPERTY(NoEdit, Transient) EPlayState PlayState = EPlayState::Stopped;
};
