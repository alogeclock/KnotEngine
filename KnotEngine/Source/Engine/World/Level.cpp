#include "World/Level.h"
#include "Component/Component.h"
#include "World/World.h"

ULevel::ULevel(UWorld& World) : OwningWorld(&World)
{
}

ULevel::~ULevel()
{
	for (const auto& Node : Nodes)
	{
		GUObjectManager.Destroy(Node.Get());
	}
	check(TickComponents.empty());
}

UWorld& ULevel::GetWorld() const
{
	check(OwningWorld);
	return *OwningWorld;
}

// 소유 배열에서 Node를 추가한 뒤 Node의 수명 주기를 실행한다.
UNode& ULevel::CreateNode(FName Name)
{
	UNode* Node = GUObjectManager.Create<UNode>(*this, Name);
	Node->LevelIndex = Nodes.size();
	Nodes.emplace_back(Node);
	if (GetWorld().GetPlayState() != EPlayState::Stopped)
	{
		Node->BeginPlay();
	}
	return *Node;
}

// 소유 배열에서 Node를 제거한 뒤 Node와 Component의 수명 주기를 종료한다.
void ULevel::RemoveNode(UNode& Node)
{
	check(&Node.GetLevel() == this);
	check(Node.LevelIndex < Nodes.size());
	check(Nodes[Node.LevelIndex].Get() == &Node);

	const SIZE_T RemoveIndex = Node.LevelIndex;
	const SIZE_T LastIndex = Nodes.size() - 1;
	if (RemoveIndex != LastIndex)
	{
		Nodes[RemoveIndex] = Nodes[LastIndex];
		Nodes[RemoveIndex]->LevelIndex = RemoveIndex;
	}
	Nodes.pop_back();
	Node.LevelIndex = UNode::InvalidLevelIndex;
	GUObjectManager.Destroy(&Node);
}

void ULevel::BeginPlay()
{
	for (const TObjectPtr<UNode>& Node : Nodes)
	{
		Node->BeginPlay();
	}
}

void ULevel::EndPlay()
{
	for (const TObjectPtr<UNode>& Node : Nodes)
	{
		Node->EndPlay();
	}
}

void ULevel::Tick(float DeltaTime)
{
	for (SIZE_T TickIndex = 0; TickIndex < TickComponents.size();)
	{
		UComponent* Component = TickComponents[TickIndex];
		Component->TickComponent(DeltaTime);
		if (TickIndex < TickComponents.size() && TickComponents[TickIndex] == Component)
		{
			++TickIndex;
		}
	}
}

// 활성화된 Tick Component를 밀집 배열 끝에 한 번만 등록한다.
void ULevel::RegisterTickComponent(UComponent& Component)
{
	check(&Component.GetOwner().GetLevel() == this);
	if (Component.TickComponentIndex != UComponent::InvalidIndex)
	{
		return;
	}

	Component.TickComponentIndex = TickComponents.size();
	TickComponents.push_back(&Component);
}

// Tick Component를 swap-pop으로 제거하고 이동한 Component의 Index를 갱신한다.
void ULevel::UnregisterTickComponent(UComponent& Component)
{
	if (Component.TickComponentIndex == UComponent::InvalidIndex)
	{
		return;
	}

	const SIZE_T RemoveIndex = Component.TickComponentIndex;
	const SIZE_T LastIndex = TickComponents.size() - 1;
	check(RemoveIndex < TickComponents.size() && TickComponents[RemoveIndex] == &Component);
	if (RemoveIndex != LastIndex)
	{
		TickComponents[RemoveIndex] = TickComponents[LastIndex];
		TickComponents[RemoveIndex]->TickComponentIndex = RemoveIndex;
	}

	TickComponents.pop_back();
	Component.TickComponentIndex = UComponent::InvalidIndex;
}
