#include "Component/Component.h"

#include "Core/Assert.h"
#include "Object/Property.h"
#include "World/Level.h"
#include "World/Node.h"
#include "World/World.h"

UNode& UComponent::GetOwner() const
{
	check(Owner);
	return *Owner;
}

UWorld& UComponent::GetWorld() const
{
	return GetOwner().GetWorld();
}

UTransformComponent& UComponent::GetTransform() const
{
	return GetOwner().GetTransform();
}

void UComponent::RegisterComponent()
{
	check(Owner && !bIsRegistered && !bHasBegunPlay && !bIsActive);
	bIsRegistered = true;
	OnRegister();
	UpdateTickRegistration();
}

void UComponent::UnregisterComponent()
{
	if (!bIsRegistered)
	{
		return;
	}

	if (bHasBegunPlay)
	{
		EndPlay();
	}

	check(!bIsActive);
	UpdateTickRegistration();
	OnUnregister();
	bIsRegistered = false;
}

void UComponent::Activate()
{
	check(bIsRegistered && bHasBegunPlay);
	if (bIsActive)
	{
		return;
	}

	bIsActive = true;
	UpdateTickRegistration();
	OnActivated();
}

void UComponent::Deactivate()
{
	if (!bIsActive)
	{
		return;
	}

	bIsActive = false;
	UpdateTickRegistration();
	OnDeactivated();
}

void UComponent::BeginPlay()
{
	check(Owner && bIsRegistered && !bHasBegunPlay);
	bHasBegunPlay = true;
	if (bAutoActivate)
	{
		Activate();
	}
}

void UComponent::EndPlay()
{
	check(bHasBegunPlay);
	Deactivate();
	bHasBegunPlay = false;
}

// Inspector에서 Tick 활성화가 변경되면 Level의 밀집 Tick 배열을 즉시 동기화한다.
void UComponent::PostEditProperty(const FProperty& Property)
{
	Super::PostEditProperty(Property);
	static const FName TickEnablePropertyName("bTickEnable");
	if (Property.GetFName() == TickEnablePropertyName)
	{
		UpdateTickRegistration();
	}
}

// Transaction 복원 후 Tick Registry를 현재 프로퍼티 상태와 다시 일치시킨다.
void UComponent::PostEditUndo()
{
	Super::PostEditUndo();
	UpdateTickRegistration();
}

// 실제로 이번 Play Session에서 Tick할 수 있는 Component만 Level에 등록한다.
void UComponent::UpdateTickRegistration()
{
	if (!Owner)
	{
		check(TickComponentIndex == InvalidIndex);
		return;
	}

	ULevel& Level = Owner->GetLevel();
	if (bCanEverTick && bTickEnable && bIsRegistered && bHasBegunPlay && bIsActive)
	{
		Level.RegisterTickComponent(*this);
	}
	else
	{
		Level.UnregisterTickComponent(*this);
	}
}
