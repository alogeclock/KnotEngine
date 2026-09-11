#pragma once

#include "Component/Component.h"
#include "Core/Geometry/Transform.h"
#include "Core/Math/Matrix.h"
#include "Core/Math/Rotator.h"

UCLASS()
class ENGINE_API UTransformComponent final : public UComponent
{
	GENERATED_CLASS(UTransformComponent, UComponent)

public:
	~UTransformComponent() override;

	const FTransform& GetRelativeTransform() const { return RelativeTransform; }
	void SetRelativeTransform(const FTransform& Transform);
	void PostEditProperty(const FProperty& Property) override;

	FMatrix GetWorldMatrix() const;
	FVector GetWorldLocation() const;
	UTransformComponent* GetParent() const { return Parent; }
	const TArray<TObjectPtr<UTransformComponent>>& GetChildren() const { return Children; }

	// 상대 변환을 유지한다. 순환을 만드는 외부 부착 요청은 false로 거절한다.
	bool SetParent(UTransformComponent* NewParent);

private:
	void OnTransformChanged();
	UPROPERTY(Category = "Transform") FTransform RelativeTransform;
	UPROPERTY(NoEdit, Transient) TObjectPtr<UTransformComponent> Parent;
	UPROPERTY(NoEdit, Transient) TArray<TObjectPtr<UTransformComponent>> Children;
};
