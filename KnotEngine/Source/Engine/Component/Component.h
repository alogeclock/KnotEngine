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

	bool IsTickable() const { return bIsTickable; }
	bool IsActive() const { return bIsActive; }
	void SetTickable(bool bInTickable) { bIsTickable = bInTickable; }

	virtual void BeginPlay();
	virtual void EndPlay();
	virtual void TickComponent(float DeltaTime) {}

protected:
	bool bIsTickable = true;

private:
	friend class UNode;

	UPROPERTY(NoEdit, Transient) TObjectPtr<UNode> Owner;
	bool bIsActive = false;
};
