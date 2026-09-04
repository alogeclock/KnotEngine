#pragma once

#include "Components/ActorComponent.h"
#include "Core/Geometry/Transform.h"
#include "Core/Math/Rotator.h"
#include "Core/Math/Vector.h"

UCLASS()
class USceneComponent : public UActorComponent
{
	GENERATED_CLASS(USceneComponent, UActorComponent)

public:
	bool IsVisible() const { return bVisible; }
	void SetVisible(bool bInVisible) { bVisible = bInVisible; }

private:
	UPROPERTY(NoEdit, Transient) TObjectPtr<USceneComponent> AttachParent;
	UPROPERTY(NoEdit, Transient) TArray<TObjectPtr<USceneComponent>> AttachChildren;

	UPROPERTY(Category = "Transform") FVector RelativeLocation = FVector::ZeroVector;
	UPROPERTY(Category = "Transform") FRotator RelativeRotation = FRotator::ZeroRotator;
	UPROPERTY(Category = "Transform") FVector RelativeScale = FVector::OneVector;

	FTransform WorldTransform = FTransform::Identity;

	bool bWorldTransformDirty = false;

	UPROPERTY() bool bVisible = true;
};
