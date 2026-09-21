#pragma once

#include "Object/Object.h"
#include "Component/TransformComponent.h"
#include "Core/Name.h"

class ULevel;
class UWorld;
class UTransformComponent;
class UComponent;
class UClass;
class FRenderer;
struct FEditorSelection;

// Level에 배치되는 최소 단위 객체로, Component의 합성을 통해 기능을 구현한다.
// 모든 Node는 생성과 함께 정확히 하나의 TransformComponent를 소유하며, 이를 제거하거나 추가할 수 없다.
// TransformComponent도 Components 배열에 포함되고, 렌더링과 게임 동작은 함께 소유한 다른 Component를 조합하여 구성한다.
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

	template <typename T, typename... Args>
	T& AddComponent(Args&&... Arguments)
	{
		static_assert(std::is_base_of_v<UComponent, T>);
		static_assert(!std::is_same_v<UTransformComponent, T>, "Nodes already own their only TransformComponent.");
		T* Component = GUObjectManager.Create<T>(std::forward<Args>(Arguments)...);
		AttachComponent(*Component);
		return *Component;
	}

	UComponent& AddComponent(const UClass& ComponentClass);
	void RemoveComponent(UComponent& Component);

	bool IsSelected() const { return bSelected; }

private:
	friend class ULevel;
	friend struct FEditorSelection;

	static constexpr SIZE_T InvalidLevelIndex = static_cast<SIZE_T>(-1);

	void AttachComponent(UComponent& Component);
	void SetSelected(bool bSelected);

	UPROPERTY(Category = "Node") FName Name;

	UPROPERTY(NoEdit, Transient) TObjectPtr<ULevel> OwningLevel;
	UPROPERTY(NoEdit, Transient) TObjectPtr<UTransformComponent> Transform;
	UPROPERTY(NoEdit, Transient) TArray<TObjectPtr<UComponent>> Components;

	SIZE_T LevelIndex = InvalidLevelIndex; // Level의 밀집 Node 배열에서 현재 위치. 외부 식별자로 사용하지 않는다.
	bool bSelected = false; // Editor 선택 원본은 FEditorSelection이 관리하며 Proxy 생성 시 초기 상태로 사용한다.
};
