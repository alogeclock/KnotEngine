#pragma once

#include "Object/Object.h"
#include "Component/TransformComponent.h"
#include "Core/Name.h"

class ULevel;
class UWorld;
class UTransformComponent;
class UComponent;
class URenderer;

UCLASS()
class ENGINE_API UNode final : public UObject
{
	GENERATED_CLASS(UNode, UObject)

public:
	explicit UNode(ULevel& Level, FName InName);
	~UNode() override;

	ULevel& GetLevel() const;
	UWorld& GetWorld() const; // GetWorld()가 유효하도록 외부에 공개된 Node는 반드시 Level에 속한다.

	FName GetName() const { return Name; }
	UTransformComponent& GetTransform() const { return *Transform; }
	const TArray<TObjectPtr<UComponent>>& GetComponents() const { return Components; }

	void BeginPlay();
	void EndPlay();
	void Tick(float DeltaTime);
	void Render(URenderer& Renderer, const FMatrix& ViewProjection) const;

	template <typename T, typename... Args>
	T& AddComponent(Args&&... Arguments)
	{
		static_assert(std::is_base_of_v<UComponent, T>);
		static_assert(!std::is_same_v<UTransformComponent, T>, "Nodes already own their only TransformComponent.");
		T* Component = GUObjectManager.Create<T>(std::forward<Args>(Arguments)...);
		AttachComponent(*Component);
		return *Component;
	}

private:
	void AttachComponent(UComponent& Component);

	UPROPERTY() FName Name;

	UPROPERTY(NoEdit, Transient) TObjectPtr<ULevel> OwningLevel;
	UPROPERTY(NoEdit, Transient) TObjectPtr<UTransformComponent> Transform;
	UPROPERTY(NoEdit, Transient) TArray<TObjectPtr<UComponent>> Components;
};
