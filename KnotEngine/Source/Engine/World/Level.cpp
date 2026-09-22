#include "World/Level.h"
#include "Component/Component.h"
#include "World/World.h"

#include <algorithm>
#include <utility>

ULevel::ULevel(UWorld& World) : OwningWorld(&World)
{
}

ULevel::~ULevel()
{
	while (!Nodes.empty())
	{
		RemoveNode(*Nodes.back());
	}
	check(RootNodes.empty());
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
	InsertRootNode(*Node, RootNodes.size());
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

	while (!Node.GetChildren().empty())
	{
		UNode& Child = Node.GetChildren().back()->GetOwner();
		RemoveNode(Child);
	}
	Node.GetTransform().Detach();

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

// 지정한 순서에 Root Node를 추가하고 영향받은 Sibling Index를 갱신한다.
void ULevel::InsertRootNode(UNode& Node, SIZE_T SiblingIndex)
{
	UTransformComponent& Transform = Node.GetTransform();

	check(&Node.GetLevel() == this && !Node.GetParent());
	check(Transform.SiblingIndex == UTransformComponent::InvalidIndex && SiblingIndex <= RootNodes.size());
	RootNodes.insert(RootNodes.begin() + SiblingIndex, &Node);

	for (SIZE_T Index = SiblingIndex; Index < RootNodes.size(); ++Index)
	{
		RootNodes[Index]->GetTransform().SiblingIndex = Index;
	}
}

// Root Node를 순서 인덱스에서 제거하고 뒷 인덱스를 보정한다.
void ULevel::RemoveRootNode(UNode& Node)
{
	UTransformComponent& Transform = Node.GetTransform();
	const SIZE_T RemoveIndex = Transform.SiblingIndex;

	check(&Node.GetLevel() == this && !Node.GetParent());
	check(RemoveIndex < RootNodes.size() && RootNodes[RemoveIndex] == &Node);
	RootNodes.erase(RootNodes.begin() + RemoveIndex);

	Transform.SiblingIndex = UTransformComponent::InvalidIndex;
	for (SIZE_T Index = RemoveIndex; Index < RootNodes.size(); ++Index)
	{
		RootNodes[Index]->GetTransform().SiblingIndex = Index;
	}
}

// RootNodes에서 Node를 이동하고 영향받은 Sibling Index를 갱신한다.
bool ULevel::SetRootSiblingIndex(UNode& Node, SIZE_T SiblingIndex)
{
	UTransformComponent& Transform = Node.GetTransform();
	check(&Node.GetLevel() == this && !Node.GetParent());
	if (SiblingIndex >= RootNodes.size())
	{
		return false;
	}

	const SIZE_T CurrentSiblingIndex = Transform.SiblingIndex;
	check(CurrentSiblingIndex < RootNodes.size() && RootNodes[CurrentSiblingIndex] == &Node);
	if (CurrentSiblingIndex == SiblingIndex)
	{
		return true;
	}

	RootNodes.erase(RootNodes.begin() + CurrentSiblingIndex);
	RootNodes.insert(RootNodes.begin() + SiblingIndex, &Node);
	const SIZE_T FirstChangedIndex = (std::min)(CurrentSiblingIndex, SiblingIndex);
	const SIZE_T LastChangedIndex = (std::max)(CurrentSiblingIndex, SiblingIndex);
	for (SIZE_T Index = FirstChangedIndex; Index <= LastChangedIndex; ++Index)
	{
		RootNodes[Index]->GetTransform().SiblingIndex = Index;
	}
	return true;
}

// 임시 World에서 복원한 Node를 대상 Level로 전송하고, Component 등록 대상 World를 교체한다.
void ULevel::TransferNodes(ULevel& Destination)
{
	check(&Destination != this && Destination.Nodes.empty() && Destination.RootNodes.empty());
	for (const TObjectPtr<UNode>& Node : Nodes)
	{
		for (const TObjectPtr<UComponent>& Component : Node->Components)
		{
			Component->UnregisterComponent();
		}
	}

	Destination.Nodes = std::move(Nodes);
	Destination.RootNodes = std::move(RootNodes);
	Nodes.clear();
	RootNodes.clear();

	for (SIZE_T Index = 0; Index < Destination.Nodes.size(); ++Index)
	{
		UNode& Node = *Destination.Nodes[Index];
		Node.OwningLevel = &Destination;
		Node.LevelIndex = Index;
	}

	for (const TObjectPtr<UNode>& Node : Destination.Nodes)
	{
		for (const TObjectPtr<UComponent>& Component : Node->Components)
		{
			Component->RegisterComponent();
		}
	}
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
