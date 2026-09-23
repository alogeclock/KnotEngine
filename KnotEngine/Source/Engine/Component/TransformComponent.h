#pragma once

#include "Component/Component.h"
#include "Core/Geometry/Transform.h"
#include "Core/Math/Matrix.h"
#include "Core/Math/Rotator.h"

UCLASS(DisplayName = "Transform")
class ENGINE_API UTransformComponent final : public UComponent
{
	GENERATED_CLASS(UTransformComponent, UComponent)

public:
	~UTransformComponent() override;

	FTransform GetRelativeTransform() const;
	const FRotator& GetRelativeRotation() const { return Rotation; }

	void SetRelativeTransform(const FTransform& Transform);
	void SetRelativeRotation(const FRotator& InRotation);
	void PostInitProperties() override;
	void PostDuplicate() override;
	void PostEditProperty(const FProperty& Property) override;

	FMatrix GetWorldMatrix() const;
	FVector GetWorldLocation() const;

	UTransformComponent* GetParent() const { return Parent; }
	const TArray<TObjectPtr<UTransformComponent>>& GetChildren() const { return Children; }
	SIZE_T GetSiblingIndex() const { return SiblingIndex; }

	bool SetParentRelative(UTransformComponent* NewParent, SIZE_T SiblingIndex = static_cast<SIZE_T>(-1));
	bool SetParentAbsolute(UTransformComponent* NewParent, SIZE_T SiblingIndex = static_cast<SIZE_T>(-1));
	bool SetSiblingIndex(SIZE_T SiblingIndex);

private:
	friend class ULevel;

	static constexpr SIZE_T InvalidIndex = static_cast<SIZE_T>(-1);

	void OnTransformChanged();
	
	void SetRelativeRotation(const FQuat& InRotation);
	void Detach();

	/// 부모 Transform을 기준으로 한 상대 위치이다.
	UPROPERTY(Category = "Transform") FVector Location = FVector::ZeroVector;

	/// 부모 Transform을 기준으로 한 상대 회전이며 사용자가 입력한 누적 각도를 보존한다.
	UPROPERTY(Category = "Transform") FRotator Rotation = FRotator::ZeroRotator;

	/// 부모 Transform을 기준으로 한 상대 크기이다.
	UPROPERTY(Category = "Transform") FVector Scale = FVector::OneVector;

	/// Rotation에 대응하는 정규화된 실제 회전 캐시이다.
	FQuat CachedRotation = FQuat::Identity;

	/// 계층 구조에서 연결된 부모 Transform이다.
	UPROPERTY(NoEdit, Transient) TObjectPtr<UTransformComponent> Parent;

	/// 계층 구조에서 연결된 자식 Transform 목록이다.
	UPROPERTY(NoEdit, Transient) TArray<TObjectPtr<UTransformComponent>> Children;

	/// Parent의 Children 또는 Level의 RootNodes 배열에서 현재 순서다.
	SIZE_T SiblingIndex = InvalidIndex; 
};
