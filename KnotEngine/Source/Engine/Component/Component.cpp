#include "Component/Component.h"

#include "Core/Assert.h"
#include "GameFramework/Node.h"
#include "GameFramework/World.h"

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

void UComponent::BeginPlay()
{
	check(Owner && !bIsActive);
	bIsActive = true;
}

void UComponent::EndPlay()
{
	check(bIsActive);
	bIsActive = false;
}
