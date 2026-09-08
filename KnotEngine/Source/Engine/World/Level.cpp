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

UNode& ULevel::CreateNode(FName Name)
{
	UNode* Node = GUObjectManager.Create<UNode>(*this, Name);
	Nodes.emplace_back(Node);
	if (GetWorld().GetPlayState() != EPlayState::Stopped)
	{
		Node->BeginPlay();
	}
	return *Node;
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

void ULevel::Render(URenderer& Renderer, const FMatrix& ViewProjection) const
{
	for (const TObjectPtr<UNode>& Node : Nodes)
	{
		Node->Render(Renderer, ViewProjection);
	}
}
