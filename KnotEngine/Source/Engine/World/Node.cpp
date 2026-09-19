#include "World/Node.h"
#include "Component/Component.h"
#include "Component/PrimitiveComponent.h"
#include "Object/Class.h"
#include "World/Level.h"
#include "World/World.h"

UNode::UNode(ULevel& Level, FName InName) : OwningLevel(&Level), Name(InName)
{
	Transform = GUObjectManager.Create<UTransformComponent>();
	AttachComponent(*Transform);
}

UNode::~UNode()
{
	EndPlay();
	for (auto Iterator = Components.rbegin(); Iterator != Components.rend(); ++Iterator)
	{
		(*Iterator)->UnregisterComponent();
		GUObjectManager.Destroy(Iterator->Get());
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

void UNode::Tick(float DeltaTime)
{
	for (const TObjectPtr<UComponent>& Component : Components)
	{
		if (GetWorld().GetPlayState() != EPlayState::Playing)
		{
			break;
		}
		if (Component->IsActive() && Component->IsTickEnabled())
		{
			Component->TickComponent(DeltaTime);
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
UComponent& UNode::AddComponent(const UClass& ComponentClass)
{
	panic(ComponentClass.IsChildOf(UComponent::StaticClass()));
	panic(&ComponentClass != UTransformComponent::StaticClass());
	panic(ComponentClass.CanCreateObject());

	UObject* Object = ComponentClass.CreateObject();
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

	Component.Owner = this;
	Components.emplace_back(&Component);
	Component.RegisterComponent();

	if (GetWorld().GetPlayState() != EPlayState::Stopped)
	{
		Component.BeginPlay();
	}
}
