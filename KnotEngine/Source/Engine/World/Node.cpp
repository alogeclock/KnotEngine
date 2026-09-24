#include "World/Node.h"
#include "Component/Component.h"
#include "Component/PrimitiveComponent.h"
#include "Object/Reflection/Class.h"
#include "Object/Property.h"
#include "World/Level.h"
#include "World/World.h"

UNode::UNode()
{
	Transform = CreateDefaultSubobject<UTransformComponent>(FName("Transform"));
}

UNode::~UNode()
{
	EndPlay();
	for (auto Iterator = Components.rbegin(); Iterator != Components.rend(); ++Iterator)
	{
		(*Iterator)->UnregisterComponent();
		if ((*Iterator)->HasAnyFlags(EObjectFlags::DefaultSubobject))
		{
			RemoveDefaultSubobject(*Iterator->Get());
		}
		GUObjectManager.Destroy(Iterator->Get());
	}
}

// Inspector에서 변경된 Node 이름을 World의 다음 접미사 맵에 반영한다.
void UNode::PostEditProperty(const FProperty& Property)
{
	Super::PostEditProperty(Property);
	static const FName NamePropertyName("Name");
	if (OwningLevel && Property.GetFName() == NamePropertyName)
	{
		GetWorld().RegisterNodeName(Name.ToString());
	}
}

// Transaction으로 복원된 이름을 World의 고유 이름 카운터에 다시 반영한다.
void UNode::PostEditUndo()
{
	Super::PostEditUndo();
	if (OwningLevel)
	{
		GetWorld().RegisterNodeName(Name.ToString());
	}
}

ULevel& UNode::GetLevel() const
{
	check(OwningLevel);
	return *OwningLevel;
}

UWorld& UNode::GetWorld() const
{
	return GetLevel().GetWorld();
}

void UNode::BeginPlay()
{
	for (const TObjectPtr<UComponent>& Component : Components)
	{
		if (!Component->HasBegunPlay())
		{
			Component->BeginPlay();
		}
	}
}

void UNode::EndPlay()
{
	for (const TObjectPtr<UComponent>& Component : Components)
	{
		if (Component->HasBegunPlay())
		{
			Component->EndPlay();
		}
	}
}

// Node가 직접 소유하는 Primitive Component의 Scene Proxy에 선택 상태를 Push한다.
void UNode::SetSelected(bool bInSelected)
{
	if (bSelected == bInSelected)
	{
		return;
	}

	bSelected = bInSelected;
	for (const TObjectPtr<UComponent>& Component : Components)
	{
		if (Component && Component->IsA(UPrimitiveComponent::StaticClass()))
		{
			static_cast<UPrimitiveComponent*>(Component.Get())->PushSelection(bInSelected);
		}
	}
}

// 기본 생성 가능한 리플렉션 클래스로 Component를 생성하여 이 Node의 수명 주기에 연결한다.
UComponent& UNode::AddComponent(const UClass& ComponentClass, FName Name)
{
	panic(ComponentClass.IsChildOf(UComponent::StaticClass()));
	panic(&ComponentClass != UTransformComponent::StaticClass());
	panic(ComponentClass.CanCreateObject());

	UObject* Object = GUObjectManager.NewObject(*const_cast<UClass*>(&ComponentClass), this, std::move(Name));
	UComponent* Component = static_cast<UComponent*>(Object);
	AttachComponent(*Component, Components.size());
	return *Component;
}

// Transform을 제외한 소유 Component를 분리하고 즉시 파괴한다.
void UNode::RemoveComponent(UComponent& Component)
{
	DetachComponent(Component);
	DestroyDetachedComponent(Component);
}

// Component를 파괴하지 않고 Node와 World 실행 상태에서 분리하고 기존 배열 위치를 반환한다.
SIZE_T UNode::DetachComponent(UComponent& Component)
{
	panic(&Component != Transform.Get());
	check(Component.Owner.Get() == this);
	for (SIZE_T Index = 0; Index < Components.size(); ++Index)
	{
		if (Components[Index].Get() != &Component)
		{
			continue;
		}
		Component.UnregisterComponent();
		Component.Owner = nullptr;
		Components.erase(Components.begin() + Index);
		return Index;
	}
	panicf(false, "이 Node가 소유하지 않은 Component를 분리할 수 없다.");
}

// Component를 지정한 배열 위치에 연결하고 Node가 Level에 있으면 World에 등록한다.
void UNode::AttachComponent(UComponent& Component, SIZE_T ComponentIndex)
{
	check(!Component.IsOwned() && !Component.IsRegistered() && ComponentIndex <= Components.size());
	check(!Component.GetOuter() || Component.GetOuter() == this);
	Component.Owner = this;
	Components.insert(Components.begin() + ComponentIndex, &Component);
	if (OwningLevel && IsInLevel())
	{
		Component.RegisterComponent();
		if (GetWorld().GetPlayState() != EPlayState::Stopped)
		{
			Component.BeginPlay();
		}
	}
}

// 분리된 Component를 Default Subobject 목록에서도 제거한 뒤 파괴한다.
void UNode::DestroyDetachedComponent(UComponent& Component)
{
	check(!Component.Owner && !Component.IsRegistered());
	if (Component.HasAnyFlags(EObjectFlags::DefaultSubobject))
	{
		RemoveDefaultSubobject(Component);
	}
	GUObjectManager.Destroy(&Component);
}

// 완전히 초기화된 Node가 Level에 연결된 뒤 모든 생성자 Component를 World에 등록한다.
void UNode::RegisterComponents()
{
	check(OwningLevel);
	for (const TObjectPtr<UComponent>& Component : Components)
	{
		check(Component && Component->Owner.Get() == this && !Component->IsRegistered());
		Component->RegisterComponent();
	}
}

// Node의 Transform 계층에 연결된 Parent를 Node 내부 계층 접근으로 노출한다.
UTransformComponent* UNode::GetParent() const
{
	return Transform->GetParent();
}

// Node의 Transform 계층에 연결된 Children을 Node 내부 계층 접근으로 노출한다.
const TArray<TObjectPtr<UTransformComponent>>& UNode::GetChildren() const
{
	return Transform->GetChildren();
}

// Node의 Transform에 캐시된 Hierarchy Sibling Index를 반환한다.
SIZE_T UNode::GetSiblingIndex() const
{
	return Transform->GetSiblingIndex();
}
