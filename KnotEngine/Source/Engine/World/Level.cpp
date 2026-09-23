#include "World/Level.h"
#include "Component/Component.h"
#include "Object/Reflection/Class.h"
#include "Object/ObjectInstancingContext.h"
#include "World/World.h"

#include <algorithm>
#include <functional>
#include <utility>

// CDO 복사가 끝난 실제 Level을 Outer World에 연결한다.
void ULevel::PostInitProperties()
{
	Super::PostInitProperties();
	if (IsTemplate())
	{
		return;
	}
	panic(GetOuter() && GetOuter()->IsA(UWorld::StaticClass()));
	OwningWorld = static_cast<UWorld*>(GetOuter());
}

// 복제된 Level의 비직렬화 소유 World 참조를 Outer에서 다시 구성한다.
void ULevel::PostDuplicate()
{
	Super::PostDuplicate();
	panic(GetOuter() && GetOuter()->IsA(UWorld::StaticClass()));
	OwningWorld = static_cast<UWorld*>(GetOuter());
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
	return CreateNode(*UNode::StaticClass(), std::move(Name));
}

// 지정한 UNode 파생 클래스의 CDO 기본값과 Default Component를 복사해 Level에 배치한다.
UNode& ULevel::CreateNode(const UClass& NodeClass, FName Name)
{
	panic(NodeClass.IsChildOf(UNode::StaticClass()) && NodeClass.CanCreateObject());
	UNode* Node = static_cast<UNode*>(GUObjectManager.NewObject(*const_cast<UClass*>(&NodeClass)));
	Node->OwningLevel = this;
	Node->Name = std::move(Name);

	GetWorld().RegisterNodeName(Node->Name.ToString());
	Node->LevelIndex = Nodes.size();
	Nodes.emplace_back(Node);

	InsertRootNode(*Node, RootNodes.size());

	Node->RegisterComponents();
	if (GetWorld().GetPlayState() != EPlayState::Stopped)
	{
		Node->BeginPlay();
	}
	return *Node;
}

// 선택된 Node 계층을 한 Instancing Context로 복제하고 내부 UObject 참조와 부모 관계를 새 객체로 재매핑한다.
TArray<UNode*> ULevel::DuplicateNodes(const TArray<UNode*>& SourceNodes)
{
	FObjectInstancingContext InstancingContext;
	TMap<UNode*, FName> DuplicateNames;
	TArray<UNode*> SourceRoots;

	for (UNode* SourceNode : SourceNodes)
	{
		if (!SourceNode || &SourceNode->GetLevel() != this || std::find(SourceRoots.begin(), SourceRoots.end(), SourceNode) != SourceRoots.end())
		{
			continue;
		}

		bool bHasSelectedAncestor = false;
		for (UTransformComponent* Parent = SourceNode->GetParent(); Parent; Parent = Parent->GetParent())
		{
			if (std::find(SourceNodes.begin(), SourceNodes.end(), &Parent->GetOwner()) != SourceNodes.end())
			{
				bHasSelectedAncestor = true;
				break;
			}
		}
		if (!bHasSelectedAncestor)
		{
			SourceRoots.push_back(SourceNode);
		}
	}

	const std::function<void(UNode&)> DuplicateHierarchy = [&](UNode& SourceNode)
	{
		const FName DuplicateName = GetWorld().GetNodeName(SourceNode.GetName().ToString());
		UNode& DuplicateNode = CreateNode(*SourceNode.GetClass(), DuplicateName);
		DuplicateNames.emplace(&DuplicateNode, DuplicateName);
		InstancingContext.Add(SourceNode, DuplicateNode);

		for (const TObjectPtr<UComponent>& DuplicateComponent : DuplicateNode.Components)
		{
			DuplicateComponent->UnregisterComponent();
		}
		for (const TObjectPtr<UComponent>& SourceComponent : SourceNode.Components)
		{
			if (SourceComponent->HasAnyFlags(EObjectFlags::DefaultSubobject))
			{
				UObject* DuplicateSubobject = DuplicateNode.GetDefaultSubobject(SourceComponent->GetObjectName());
				panic(DuplicateSubobject && DuplicateSubobject->GetClass() == SourceComponent->GetClass());
				InstancingContext.Add(*SourceComponent, *DuplicateSubobject);
				continue;
			}

			UComponent& DuplicateComponent = DuplicateNode.AddComponent(*SourceComponent->GetClass(), SourceComponent->GetObjectName());
			DuplicateComponent.UnregisterComponent();
			InstancingContext.Add(*SourceComponent, DuplicateComponent);
		}

		for (const TObjectPtr<UTransformComponent>& Child : SourceNode.GetChildren())
		{
			DuplicateHierarchy(Child->GetOwner());
		}
	};
	for (UNode* SourceRoot : SourceRoots)
	{
		DuplicateHierarchy(*SourceRoot);
	}

	for (const auto& [Source, Destination] : InstancingContext.GetObjects())
	{
		Source->GetClass()->CopyProperties(Destination, Source, InstancingContext);
	}
	for (const auto& [DuplicateNode, DuplicateName] : DuplicateNames)
	{
		DuplicateNode->Name = DuplicateName;
	}

	for (UNode* SourceRoot : SourceRoots)
	{
		TArray<UNode*> Hierarchy{ SourceRoot };
		for (SIZE_T Index = 0; Index < Hierarchy.size(); ++Index)
		{
			UNode& SourceNode = *Hierarchy[Index];
			auto* DuplicateNode = static_cast<UNode*>(InstancingContext.Find(&SourceNode));
			check(DuplicateNode);
			UTransformComponent* SourceParent = SourceNode.GetParent();
			UTransformComponent* DuplicateParent = nullptr;
			if (SourceParent)
			{
				if (UObject* MappedParent = InstancingContext.Find(&SourceParent->GetOwner()))
				{
					DuplicateParent = &static_cast<UNode*>(MappedParent)->GetTransform();
				}
				else
				{
					DuplicateParent = SourceParent;
				}
			}
			panic(DuplicateNode->GetTransform().SetParentRelative(DuplicateParent));

			for (const TObjectPtr<UTransformComponent>& Child : SourceNode.GetChildren())
			{
				Hierarchy.push_back(&Child->GetOwner());
			}
		}
	}

	for (const auto& [Source, Destination] : InstancingContext.GetObjects())
	{
		Destination->PostDuplicate();
	}
	for (const auto& [DuplicateNode, DuplicateName] : DuplicateNames)
	{
		DuplicateNode->RegisterComponents();
		if (GetWorld().GetPlayState() != EPlayState::Stopped)
		{
			DuplicateNode->BeginPlay();
		}
	}

	TArray<UNode*> Result;
	Result.reserve(SourceNodes.size());
	for (UNode* SourceNode : SourceNodes)
	{
		if (UObject* Duplicate = InstancingContext.Find(SourceNode))
		{
			Result.push_back(static_cast<UNode*>(Duplicate));
		}
	}
	return Result;
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
