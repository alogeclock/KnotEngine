#include "Component/TransformComponent.h"
#include "Component/PrimitiveComponent.h"
#include "World/Level.h"
#include "World/Node.h"

#include <algorithm>
#include <cmath>

UTransformComponent::~UTransformComponent()
{
	check(Children.empty());
	Detach();
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

// 현재 Relative Transform을 유지한 채 Parent와 Sibling 순서를 변경한다.
bool UTransformComponent::SetParentRelative(UTransformComponent* NewParent, SIZE_T SiblingIndex)
{
	static constexpr SIZE_T LastSiblingIndex = static_cast<SIZE_T>(-1);
	ULevel& Level = GetOwner().GetLevel();
	if (NewParent && &NewParent->GetOwner().GetLevel() != &Level)
	{
		return false;
	}
	for (const UTransformComponent* Ancestor = NewParent; Ancestor; Ancestor = Ancestor->Parent)
	{
		if (Ancestor == this)
		{
			return false;
		}
	}
	if (Parent.Get() == NewParent)
	{
		return SiblingIndex == LastSiblingIndex || SetSiblingIndex(SiblingIndex);
	}

	const SIZE_T NewSiblingCount = NewParent ? NewParent->Children.size() : Level.RootNodes.size();
	const SIZE_T NewSiblingIndex = SiblingIndex == LastSiblingIndex ? NewSiblingCount : SiblingIndex;
	if (NewSiblingIndex > NewSiblingCount)
	{
		return false;
	}

	Detach();
	Parent = NewParent;
	if (Parent)
	{
		Parent->Children.insert(Parent->Children.begin() + NewSiblingIndex, this);
		for (SIZE_T Index = NewSiblingIndex; Index < Parent->Children.size(); ++Index)
		{
			Parent->Children[Index]->SiblingIndex = Index;
		}
	}
	else
	{
		Level.InsertRootNode(GetOwner(), NewSiblingIndex);
	}
	OnTransformChanged();
	return true;
}

// 현재 World Transform을 유지하도록 새 Parent 기준의 Relative Transform을 계산한다.
bool UTransformComponent::SetParentAbsolute(UTransformComponent* NewParent, SIZE_T SiblingIndex)
{
	static constexpr SIZE_T LastSiblingIndex = static_cast<SIZE_T>(-1);
	ULevel& Level = GetOwner().GetLevel();
	if (NewParent && &NewParent->GetOwner().GetLevel() != &Level)
	{
		return false;
	}
	for (const UTransformComponent* Ancestor = NewParent; Ancestor; Ancestor = Ancestor->Parent)
	{
		if (Ancestor == this)
		{
			return false;
		}
	}
	if (Parent.Get() == NewParent)
	{
		return SiblingIndex == LastSiblingIndex || SetSiblingIndex(SiblingIndex);
	}

	const SIZE_T NewSiblingCount = NewParent ? NewParent->Children.size() : Level.RootNodes.size();
	const SIZE_T NewSiblingIndex = SiblingIndex == LastSiblingIndex ? NewSiblingCount : SiblingIndex;
	if (NewSiblingIndex > NewSiblingCount)
	{
		return false;
	}

	FMatrix RelativeMatrix = GetWorldMatrix();
	if (NewParent)
	{
		const FMatrix ParentWorldMatrix = NewParent->GetWorldMatrix();
		if (std::fabs(ParentWorldMatrix.GetDeterminant()) <= KMath::Epsilon)
		{
			return false;
		}
		RelativeMatrix *= ParentWorldMatrix.GetInverse();
	}

	FVector Translation;
	FMatrix Rotation;
	FVector Scale;
	if (!RelativeMatrix.Decompose(Translation, Rotation, Scale))
	{
		return false;
	}
	const FTransform NewRelativeTransform(FQuat(Rotation), Translation, Scale);

	Detach();
	Parent = NewParent;
	if (Parent)
	{
		Parent->Children.insert(Parent->Children.begin() + NewSiblingIndex, this);
		for (SIZE_T Index = NewSiblingIndex; Index < Parent->Children.size(); ++Index)
		{
			Parent->Children[Index]->SiblingIndex = Index;
		}
	}
	else
	{
		Level.InsertRootNode(GetOwner(), NewSiblingIndex);
	}
	SetRelativeTransform(NewRelativeTransform);
	return true;
}

// Parent의 Children 또는 Level의 RootNodes에서 현재 Transform을 제거하고 뒷 인덱스를 보정한다.
void UTransformComponent::Detach()
{
	if (SiblingIndex == InvalidIndex)
	{
		check(!Parent);
		return;
	}
	if (Parent)
	{
		check(SiblingIndex < Parent->Children.size() && Parent->Children[SiblingIndex].Get() == this);
		const SIZE_T RemoveIndex = SiblingIndex;
		Parent->Children.erase(Parent->Children.begin() + RemoveIndex);
		for (SIZE_T Index = RemoveIndex; Index < Parent->Children.size(); ++Index)
		{
			Parent->Children[Index]->SiblingIndex = Index;
		}
		Parent = nullptr;
		SiblingIndex = InvalidIndex;
		return;
	}
	GetOwner().GetLevel().RemoveRootNode(GetOwner());
}

// 현재 Parent 그룹 안에서 Transform의 Sibling 순서를 변경한다.
bool UTransformComponent::SetSiblingIndex(SIZE_T SiblingIndex)
{
	if (!Parent)
	{
		return GetOwner().GetLevel().SetRootSiblingIndex(GetOwner(), SiblingIndex);
	}
	if (SiblingIndex >= Parent->Children.size())
	{
		return false;
	}

	const SIZE_T CurrentIndex = this->SiblingIndex;
	if (CurrentIndex == SiblingIndex)
	{
		return true;
	}
	TObjectPtr<UTransformComponent> Transform = Parent->Children[CurrentIndex];
	Parent->Children.erase(Parent->Children.begin() + CurrentIndex);
	Parent->Children.insert(Parent->Children.begin() + SiblingIndex, Transform);
	const SIZE_T FirstChangedIndex = (std::min)(CurrentIndex, SiblingIndex);
	const SIZE_T LastChangedIndex = (std::max)(CurrentIndex, SiblingIndex);
	for (SIZE_T Index = FirstChangedIndex; Index <= LastChangedIndex; ++Index)
	{
		Parent->Children[Index]->SiblingIndex = Index;
	}
	return true;
}
