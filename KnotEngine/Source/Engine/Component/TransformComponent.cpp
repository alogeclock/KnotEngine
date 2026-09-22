#include "Component/TransformComponent.h"
#include "Component/PrimitiveComponent.h"
#include "World/Node.h"

UTransformComponent::~UTransformComponent()
{
	SetParent(nullptr);
	for (const auto& Child : Children)
	{
		Child->Parent = nullptr;
		Child->OnTransformChanged();
	}
}

void UTransformComponent::SetRelativeTransform(const FTransform& Transform)
{
	Location = Transform.Translation;
	Scale = Transform.Scale;
	SetRelativeRotation(Transform.Rotation);
	OnTransformChanged();
}

// 편집용 오일러 회전을 그대로 보존하면서 실제 회전 캐시를 갱신한다.
void UTransformComponent::SetRelativeRotation(const FRotator& InRotation)
{
	Rotation = InRotation;
	CachedRotation = Rotation.Quaternion().GetNormalized();
	OnTransformChanged();
}

void UTransformComponent::PostEditProperty(const FProperty& Property)
{
	Super::PostEditProperty(Property);
	CachedRotation = Rotation.Quaternion().GetNormalized();
	OnTransformChanged();
}

// 계산용 Transform을 편집 원본 값과 Quaternion 캐시로 구성한다.
FTransform UTransformComponent::GetRelativeTransform() const
{
	return FTransform(CachedRotation, Location, Scale);
}

// 외부 Quaternion의 실제 회전 변화량만 기존 오일러 값에 더해 누적 각도를 보존한다.
void UTransformComponent::SetRelativeRotation(const FQuat& InRotation)
{
	const FQuat NewRotation = InRotation.GetNormalized();
	if (NewRotation.Equals(CachedRotation))
	{
		CachedRotation = NewRotation;
		return;
	}

	FRotator Winding;
	FRotator Remainder;
	Rotation.GetWindingAndRemainder(Winding, Remainder);

	FRotator NewRemainder = NewRotation.Rotator();
	Remainder.SetClosest(NewRemainder);
	FRotator DeltaRotation = NewRemainder - Remainder;
	DeltaRotation.Normalize();
	Rotation += DeltaRotation;
	CachedRotation = NewRotation;
}

void UTransformComponent::OnTransformChanged()
{
	// Node 파괴 시 Transform은 마지막에 제거된다. 이미 파괴된 형제 Component를 조회하지 않는다.
	if (IsRegistered())
	{
		for (const auto& Component : GetOwner().GetComponents())
		{
			if (Component->IsA(UPrimitiveComponent::StaticClass()))
			{
				static_cast<UPrimitiveComponent*>(Component.Get())->EnqueueRenderCommand(ERenderCommandType::Transform);
			}
		}
	}
	for (const auto& Child : Children)
	{
		Child->OnTransformChanged();
	}
}

FMatrix UTransformComponent::GetWorldMatrix() const
{
	// 행벡터 규약. 행렬로 합성하여 비균일 스케일과 회전에서 생기는 shear도 유지한다.
	FMatrix Result = GetRelativeTransform().ToMatrix();
	for (const UTransformComponent* Ancestor = Parent; Ancestor; Ancestor = Ancestor->Parent)
	{
		Result *= Ancestor->GetRelativeTransform().ToMatrix();
	}
	return Result;
}

FVector UTransformComponent::GetWorldLocation() const
{
	return GetWorldMatrix().TransformPosition(FVector::ZeroVector);
}

bool UTransformComponent::SetParent(UTransformComponent* NewParent)
{
	// 자신이나 자신의 자손을 부모로 수정하지 않도록 한다.
	for (const UTransformComponent* Ancestor = NewParent; Ancestor; Ancestor = Ancestor->Parent)
	{
		if (Ancestor == this)
		{
			return false;
		}
	}
	// 같은 부모라면 자식 목록 수정과 Transform 변경 통지가 필요없다.
	if (Parent.Get() == NewParent)
	{
		return true;
	}
	// 기존 부모가 더 이상 자신에게 Transform 변경을 전파하지 않도록 자식 목록에서 제거한다.
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
	OnTransformChanged(); // 자식과 자손의 렌더 상태를 갱신한다.
	return true;
}
