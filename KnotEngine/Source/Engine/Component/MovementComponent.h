#pragma once

#include "Component/Component.h"
#include "Core/Math/Rotator.h"

UCLASS()
class ENGINE_API UMovementComponent : public UComponent
{
	GENERATED_CLASS(UMovementComponent, UComponent)

public:
	UMovementComponent();
	void TickComponent(float DeltaTime) override;
	const FRotator& GetRotationRate() const { return RotationRate; }

private:
	/// 노드의 초당 회전 각도(degree)
	UPROPERTY(Category = "Movement") FRotator RotationRate;
};
