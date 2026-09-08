#include "Component/Component.h"

#include "Core/Assert.h"
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
	OnActivated();
}

void UComponent::Deactivate()
{
	if (!bIsActive)
	{
		return;
	}

	bIsActive = false;
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
