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
	AttachComponent(*Component);
	return *Component;
}

// Transform을 제외한 소유 Component를 등록 해제하고 파괴한다.
void UNode::RemoveComponent(UComponent& Component)
{
	panic(&Component != Transform.Get());
	check(Component.Owner.Get() == this);

	for (auto Iterator = Components.begin(); Iterator != Components.end(); ++Iterator)
	{
		if (Iterator->Get() != &Component)
		{
			continue;
		}

		Component.UnregisterComponent();
		Component.Owner = nullptr;
		Components.erase(Iterator);
		GUObjectManager.Destroy(&Component);
		return;
	}

	panicf(false, "이 Node가 소유하지 않은 Component를 제거할 수 없다.");
}

// Node에 이미 생성된 컴포넌트를 추가한다. 컴포넌트는 반드시 Node에 속하지 않은 상태여야 한다.
void UNode::AttachComponent(UComponent& Component)
{
	check(!Component.IsOwned() && !Component.IsRegistered());
	check(!Component.GetOuter() || Component.GetOuter() == this);

	Component.Owner = this;
	Components.emplace_back(&Component);
	if (!OwningLevel)
	{
		return;
	}
	Component.RegisterComponent();

	if (GetWorld().GetPlayState() != EPlayState::Stopped)
	{
		Component.BeginPlay();
	}
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
