#include "World/Node.h"
#include "Component/Component.h"
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
