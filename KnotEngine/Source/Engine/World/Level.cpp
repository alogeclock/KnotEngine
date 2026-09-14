#include "World/Level.h"
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
	for (const TObjectPtr<UNode>& Node : Nodes)
	{
		Node->Tick(DeltaTime);
	}
}
