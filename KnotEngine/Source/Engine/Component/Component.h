#pragma once

#include "EngineAPI.h"
#include "Object/Object.h"

class UNode;
class UWorld;
class UTransformComponent;

// 공간 정보와 렌더링 상태를 갖지 않는 컴포넌트 공통 기반.
UCLASS()
class ENGINE_API UComponent : public UObject
{
	GENERATED_CLASS(UComponent, UObject)

public:
	UNode& GetOwner() const;
	UWorld& GetWorld() const;
	UTransformComponent& GetTransform() const;

	bool IsOwned() const { return Owner != nullptr; }
	bool IsRegistered() const { return bIsRegistered; }
	bool HasBegunPlay() const { return bHasBegunPlay; }
	bool IsTickEnabled() const { return bTickEnabled; }
	bool IsActive() const { return bIsActive; }

	void RegisterComponent();
	void UnregisterComponent();
	void Activate();
	void Deactivate();

	virtual void BeginPlay();
	virtual void EndPlay();
	virtual void TickComponent(float DeltaTime) {}

protected:
	virtual void OnRegister() {}
	virtual void OnUnregister() {}
	virtual void OnActivated() {}
	virtual void OnDeactivated() {}

	UPROPERTY(Category = "Component") bool bTickEnabled = true;
	UPROPERTY(Category = "Component") bool bAutoActivate = true;

private:
	friend class UNode;

	UPROPERTY(NoEdit, Transient) TObjectPtr<UNode> Owner;
	bool bIsRegistered = false; // 현재 World의 런타임 시스템에 참가
	bool bHasBegunPlay = false; // 현재 Play Session에서 BeginPlay()가 실행
	bool bIsActive = false; // Gameplay 동작이 활성화됨
};
