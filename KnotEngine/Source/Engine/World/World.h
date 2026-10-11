#pragma once

#include "Core/CoreTypes.h"
#include "Object/Object.h"
#include "World/Level.h"
#include "Render/Scene/Scene.h"

class FRenderer;
class FMapSerializer;
class UEditorEngine;
class UPrimitiveComponent;

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

	void PostInitProperties() override;
	void PostDuplicate() override;

	ULevel& CreateLevel();
	ULevel& GetPersistentLevel() const;
	void RemoveLevel(ULevel& Level);
	void Reset();

	void Tick(float DeltaTime);
	FScene& GetScene() { return Scene; }
	const FScene& GetScene() const { return Scene; }

	UPrimitiveComponent* FindPrimitive(uint64 PrimitiveId) const;
	void RegisterPrimitive(uint64 PrimitiveId, UPrimitiveComponent& Component);
	void UnregisterPrimitive(uint64 PrimitiveId);

	FName GetNodeName(const FString& BaseName);

	EPlayState GetPlayState() const { return PlayState; }
	const TArray<TObjectPtr<ULevel>>& GetLevels() const { return Levels; }

private:
	friend class FMapSerializer;
	friend class UEditorEngine;
	friend class ULevel;
	friend class UNode;

	void BeginPlay();
	void PausePlay();
	void ResumePlay();
	void EndPlay();

	void Replace(UWorld& LoadedWorld);
	void RegisterNodeName(const FString& Name);
	static void ParseNodeName(const FString& Name, FString& OutBaseName, uint64& OutNextSuffix);

	FScene Scene;
	TMap<uint64, UPrimitiveComponent*> PrimitiveComponents; // GT가 Ray 결과의 ID로 Component를 찾는 Map이며, 등록 해제 시 해당 ID를 지운다.
	TMap<FString, uint64> NameCounters;

	UPROPERTY(NoEdit, Transient) TArray<TObjectPtr<ULevel>> Levels;
	UPROPERTY(NoEdit, Transient) TObjectPtr<ULevel> PersistentLevel;

	UPROPERTY(NoEdit, Transient) EPlayState PlayState = EPlayState::Stopped;
};
