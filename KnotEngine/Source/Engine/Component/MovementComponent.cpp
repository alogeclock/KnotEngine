#include "Component/MovementComponent.h"
#include "Component/TransformComponent.h"

UMovementComponent::UMovementComponent() : RotationRate(30.0f, 45.0f, 15.0f)
{
	bTickEnabled = true;
}

void UMovementComponent::TickComponent(float DeltaTime)
{
	FTransform Transform = GetTransform().GetRelativeTransform();

	const FVector AngularVelocity(RotationRate.Roll, RotationRate.Pitch, RotationRate.Yaw);
	const float AngularSpeed = AngularVelocity.Size();
	if (AngularSpeed <= KMath::Epsilon)
	{
		return;
	}

	// 일정한 로컬 각속도를 적용하여 프레임 분할에 따라 회전 결과가 달라지지 않게 한다.
	const FQuat RotationDelta(AngularVelocity / AngularSpeed, KMath::ToRadian(AngularSpeed * DeltaTime));
	Transform.Rotation = (Transform.Rotation * RotationDelta).GetNormalized();
	GetTransform().SetRelativeTransform(Transform);
}
