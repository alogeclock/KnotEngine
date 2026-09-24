#include "World/Level.h"
#include "Component/Component.h"
#include "Object/Reflection/Class.h"
#include "Object/ObjectInstancingContext.h"
#include "World/World.h"

#include <algorithm>
#include <cmath>
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
	AttachNode(*Node, nullptr, RootNodes.size());
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

// Node 계층을 Level에서 분리한 뒤 즉시 파괴한다.
void ULevel::RemoveNode(UNode& Node)
{
	DetachNode(Node);
	DestroyDetachedNode(Node);
}

// Node 계층을 파괴하지 않고 World 실행 및 Level 소유 배열에서 분리한다.
void ULevel::DetachNode(UNode& Node)
{
	check(&Node.GetLevel() == this && Node.IsInLevel());
	Node.GetTransform().Detach();

	TArray<UNode*> Hierarchy{ &Node };
	for (SIZE_T Index = 0; Index < Hierarchy.size(); ++Index)
	{
		for (const TObjectPtr<UTransformComponent>& Child : Hierarchy[Index]->GetChildren())
		{
			Hierarchy.push_back(&Child->GetOwner());
		}
	}

	for (auto Iterator = Hierarchy.rbegin(); Iterator != Hierarchy.rend(); ++Iterator)
	{
		UNode* HierarchyNode = *Iterator;
		for (const TObjectPtr<UComponent>& Component : HierarchyNode->Components)
		{
			Component->UnregisterComponent();
		}
		check(HierarchyNode->LevelIndex < Nodes.size() && Nodes[HierarchyNode->LevelIndex].Get() == HierarchyNode);
		const SIZE_T RemoveIndex = HierarchyNode->LevelIndex;
		const SIZE_T LastIndex = Nodes.size() - 1;
		if (RemoveIndex != LastIndex)
		{
			Nodes[RemoveIndex] = Nodes[LastIndex];
			Nodes[RemoveIndex]->LevelIndex = RemoveIndex;
		}
		Nodes.pop_back();
		HierarchyNode->LevelIndex = UNode::InvalidLevelIndex;
	}
}

// 분리된 Node 계층을 같은 객체와 UUID로 Level에 다시 연결한다.
void ULevel::AttachNode(UNode& Node, UNode* Parent, SIZE_T SiblingIndex)
{
	check(&Node.GetLevel() == this && !Node.IsInLevel());
	check(!Parent || (&Parent->GetLevel() == this && Parent->IsInLevel()));

	TArray<UNode*> Hierarchy{ &Node };
	for (SIZE_T Index = 0; Index < Hierarchy.size(); ++Index)
	{
		for (const TObjectPtr<UTransformComponent>& Child : Hierarchy[Index]->GetChildren())
		{
			Hierarchy.push_back(&Child->GetOwner());
		}
	}

	for (UNode* HierarchyNode : Hierarchy)
	{
		check(&HierarchyNode->GetLevel() == this && !HierarchyNode->IsInLevel());
		HierarchyNode->LevelIndex = Nodes.size();
		Nodes.emplace_back(HierarchyNode);
	}

	panic(Node.GetTransform().SetParentRelative(Parent ? &Parent->GetTransform() : nullptr, SiblingIndex));
	for (UNode* HierarchyNode : Hierarchy)
	{
		HierarchyNode->RegisterComponents();
		if (GetWorld().GetPlayState() != EPlayState::Stopped)
		{
			HierarchyNode->BeginPlay();
		}
	}
}

// Level에서 분리된 Node 계층을 실제로 파괴한다.
void ULevel::DestroyDetachedNode(UNode& Node)
{
	check(&Node.GetLevel() == this && !Node.IsInLevel() && !Node.GetParent());
	while (!Node.GetChildren().empty())
	{
		UNode& Child = Node.GetChildren().back()->GetOwner();
		Child.GetTransform().Detach();
		DestroyDetachedNode(Child);
	}
	GUObjectManager.Destroy(&Node);
}

// 같은 Level의 Node들을 순서대로 옮기면서 World Transform과 입력 순서를 유지한다.
bool ULevel::ReparentNodesAbsolute(const TArray<UNode*>& NodesToMove, UNode* NewParent, SIZE_T SiblingIndex)
{
	// 대상과 목적지가 같은 Level에 있으며 계층 순환이 생기지 않는지 먼저 검증한다.
	if (NodesToMove.empty())
	{
		return true;
	}
	if (NewParent && (&NewParent->GetLevel() != this || !NewParent->IsInLevel()))
	{
		return false;
	}
	UTransformComponent* ParentTransform = NewParent ? &NewParent->GetTransform() : nullptr;
	const SIZE_T DestinationCount = ParentTransform ? ParentTransform->Children.size() : RootNodes.size();
	if (SiblingIndex > DestinationCount)
	{
		return false;
	}

	TSet<UTransformComponent*> MovingTransforms;
	MovingTransforms.reserve(NodesToMove.size());
	bool bChangesParent = false;
	for (UNode* Node : NodesToMove)
	{
		if (!Node || &Node->GetLevel() != this || !Node->IsInLevel())
		{
			return false;
		}
		UTransformComponent& Transform = Node->GetTransform();
		if (!MovingTransforms.emplace(&Transform).second)
		{
			return false;
		}
		bChangesParent |= Transform.Parent.Get() != ParentTransform;
	}
	for (UTransformComponent* Transform : MovingTransforms)
	{
		for (UTransformComponent* Ancestor = Transform->Parent; Ancestor; Ancestor = Ancestor->Parent)
		{
			if (MovingTransforms.contains(Ancestor))
			{
				return false;
			}
		}
	}
	for (UTransformComponent* Ancestor = ParentTransform; Ancestor; Ancestor = Ancestor->Parent)
	{
		if (MovingTransforms.contains(Ancestor))
		{
			return false;
		}
	}

	// 모든 World Transform을 변경 전에 새 부모 기준 상대 Transform으로 변환한다.
	FMatrix ParentInverse = FMatrix::Identity;
	if (bChangesParent && ParentTransform)
	{
		const FMatrix ParentWorld = ParentTransform->GetWorldMatrix();
		if (std::fabs(ParentWorld.GetDeterminant()) <= KMath::Epsilon)
		{
			return false;
		}
		ParentInverse = ParentWorld.GetInverse();
	}
	TArray<FTransform> RelativeTransforms;
	TArray<bool> ParentChanges;
	RelativeTransforms.reserve(NodesToMove.size());
	ParentChanges.reserve(NodesToMove.size());
	for (UNode* Node : NodesToMove)
	{
		UTransformComponent& Transform = Node->GetTransform();
		const bool bParentChanged = Transform.Parent.Get() != ParentTransform;
		ParentChanges.push_back(bParentChanged);
		if (!bParentChanged)
		{
			RelativeTransforms.push_back(Transform.GetRelativeTransform());
			continue;
		}
		FMatrix RelativeMatrix = Transform.GetWorldMatrix();
		if (ParentTransform)
		{
			RelativeMatrix *= ParentInverse;
		}
		FVector Translation;
		FMatrix Rotation;
		FVector Scale;
		if (!RelativeMatrix.Decompose(Translation, Rotation, Scale))
		{
			return false;
		}
		RelativeTransforms.emplace_back(FQuat(Rotation), Translation, Scale);
	}

	// 같은 목적지 배열에서 빠질 Node를 고려해 삽입 지점을 보정한다.
	SIZE_T DestinationIndex = SiblingIndex;
	bool bRootAffected = ParentTransform == nullptr;
	TSet<UTransformComponent*> AffectedParents;
	if (ParentTransform)
	{
		AffectedParents.emplace(ParentTransform);
	}
	for (UNode* Node : NodesToMove)
	{
		UTransformComponent& Transform = Node->GetTransform();
		if (Transform.Parent.Get() == ParentTransform && Transform.SiblingIndex < SiblingIndex)
		{
			--DestinationIndex;
		}
		if (Transform.Parent)
		{
			AffectedParents.emplace(Transform.Parent.Get());
		}
		else
		{
			bRootAffected = true;
		}
	}
	if (!bChangesParent)
	{
		bool bAlreadyPlaced = true;
		for (SIZE_T Index = 0; Index < NodesToMove.size(); ++Index)
		{
			if (NodesToMove[Index]->GetTransform().SiblingIndex != DestinationIndex + Index)
			{
				bAlreadyPlaced = false;
				break;
			}
		}
		if (bAlreadyPlaced)
		{
			return true;
		}
	}
	// 각 원본 형제 배열에서 이동 대상을 한 번에 제거하고 목적지에 입력 순서로 삽입한다.
	if (bRootAffected)
	{
		std::erase_if(RootNodes, [&MovingTransforms](UNode* Node)
		{
			return MovingTransforms.contains(&Node->GetTransform());
		});
	}
	for (UTransformComponent* Parent : AffectedParents)
	{
		std::erase_if(Parent->Children, [&MovingTransforms](const TObjectPtr<UTransformComponent>& Child)
		{
			return MovingTransforms.contains(Child.Get());
		});
	}
	for (UNode* Node : NodesToMove)
	{
		Node->GetTransform().Parent = ParentTransform;
	}
	if (ParentTransform)
	{
		TArray<TObjectPtr<UTransformComponent>> InsertedTransforms;
		InsertedTransforms.reserve(NodesToMove.size());
		for (UNode* Node : NodesToMove)
		{
			InsertedTransforms.emplace_back(&Node->GetTransform());
		}
		ParentTransform->Children.insert(ParentTransform->Children.begin() + DestinationIndex, InsertedTransforms.begin(), InsertedTransforms.end());
	}
	else
	{
		RootNodes.insert(RootNodes.begin() + DestinationIndex, NodesToMove.begin(), NodesToMove.end());
	}
	// 영향받은 배열의 SiblingIndex를 한 번씩 갱신한 뒤 변경된 Transform을 전파한다.
	if (bRootAffected)
	{
		for (SIZE_T Index = 0; Index < RootNodes.size(); ++Index)
		{
			RootNodes[Index]->GetTransform().SiblingIndex = Index;
		}
	}
	for (UTransformComponent* Parent : AffectedParents)
	{
		for (SIZE_T Index = 0; Index < Parent->Children.size(); ++Index)
		{
			Parent->Children[Index]->SiblingIndex = Index;
		}
	}
	for (SIZE_T Index = 0; Index < NodesToMove.size(); ++Index)
	{
		if (!ParentChanges[Index])
		{
			continue;
		}
		NodesToMove[Index]->GetTransform().SetRelativeTransform(RelativeTransforms[Index]);
	}
	return true;
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
