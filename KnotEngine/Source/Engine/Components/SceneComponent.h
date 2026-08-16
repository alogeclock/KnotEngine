#pragma once

#include "Components/ActorComponent.h"
#include "Core/Geometry/Transform.h"
#include "Core/Math/Rotator.h"
#include "Core/Math/Vector.h"

class USceneComponent : public UActorComponent
{
public:
	bool IsVisible() const { return bVisible; }
	void SetVisible(uint8 bInVisible) { bVisible = bInVisible; }

private:
	TObjectPtr<USceneComponent> AttachParent;
	TArray<TObjectPtr<USceneComponent>> AttachChildren;

	FVector RelativeLocation = FVector::ZeroVector;
	FRotator RelativeRotation = FRotator::ZeroRotator;
	FVector RelativeScale = FVector::OneVector;

	FTransform WorldTransform = FTransform::Identity;

	uint8 bWorldTransformDirty = false;
	uint8 bVisible = true;
};
