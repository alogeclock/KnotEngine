#include "Component/TransformComponent.h"

UTransformComponent::~UTransformComponent()
{
	SetParent(nullptr);
	for (const auto& Child : Children)
	{
		Child->Parent = nullptr;
	}
}

void UTransformComponent::SetRelativeTransform(const FTransform& Transform)
{
	RelativeTransform = Transform;
	RelativeTransform.Rotation.Normalize();
}

FMatrix UTransformComponent::GetWorldMatrix() const
{
	// 행벡터 규약. 행렬로 합성하여 비균일 스케일과 회전에서 생기는 shear도 유지한다.
	FMatrix Result = RelativeTransform.ToMatrix();
	for (const UTransformComponent* Ancestor = Parent; Ancestor; Ancestor = Ancestor->Parent)
	{
		Result *= Ancestor->RelativeTransform.ToMatrix();
	}
	return Result;
}

FVector UTransformComponent::GetWorldLocation() const
{
	return GetWorldMatrix().TransformPosition(FVector::ZeroVector);
}

bool UTransformComponent::SetParent(UTransformComponent* NewParent)
{
	for (const UTransformComponent* Ancestor = NewParent; Ancestor; Ancestor = Ancestor->Parent)
	{
		if (Ancestor == this)
		{
			return false;
		}
	}
	if (Parent.Get() == NewParent)
	{
		return true;
	}
	if (Parent)
	{
		for (SIZE_T Index = 0; Index < Parent->Children.size(); ++Index)
		{
			if (Parent->Children[Index].Get() == this)
			{
				Parent->Children.erase(Parent->Children.begin() + Index);
				break;
			}
		}
	}
	Parent = NewParent;
	if (Parent)
	{
		Parent->Children.emplace_back(this);
	}
	return true;
}
