#include "GameFramework/Node.h"
#include "GameFramework/Level.h"
#include "GameFramework/World.h"
#include "Component/RendererComponent.h"

UNode::UNode(ULevel& Level, FName InName) : OwningLevel(&Level), Name(InName)
{
	Transform = GUObjectManager.Create<UTransformComponent>();
	Transform->Owner = this;
	Components.emplace_back(Transform.Get());
}

UNode::~UNode()
{
	EndPlay();
	for (auto Iterator = Components.rbegin(); Iterator != Components.rend(); ++Iterator)
	{
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
	for (const auto& Component : Components)
	{
		if (!Component->IsActive())
		{
			Component->BeginPlay();
		}
	}
}

void UNode::EndPlay()
{
	for (const auto& Component : Components)
	{
		if (Component->IsActive())
		{
			Component->EndPlay();
		}
	}
}

void UNode::Tick(float DeltaTime)
{
	for (const auto& Component : Components)
	{
		if (GetWorld().GetPlayState() != EPlayState::Playing)
		{
			break;
		}
		if (Component->IsActive() && Component->IsTickable())
		{
			Component->TickComponent(DeltaTime);
		}
	}
}

void UNode::Render(URenderer& Renderer, const FMatrix& ViewProjection) const
{
	for (const auto& Component : Components)
	{
		if (Component->IsA(URendererComponent::StaticClass()))
		{
			static_cast<URendererComponent*>(Component.Get())->Render(Renderer, ViewProjection);
		}
	}
}

// Node에 이미 생성된 컴포넌트를 추가한다. 컴포넌트는 반드시 Node에 속하지 않은 상태여야 한다.
void UNode::AttachComponent(UComponent& Component)
{
	check(!Component.Owner);
	Component.Owner = this;
	Components.emplace_back(&Component);
	if (GetWorld().GetPlayState() != EPlayState::Stopped)
	{
		Component.BeginPlay();
	}
}
