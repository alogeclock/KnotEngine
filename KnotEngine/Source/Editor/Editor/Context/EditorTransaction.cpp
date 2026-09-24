#include "Editor/Context/EditorTransaction.h"

#include "Component/TransformComponent.h"
#include "Component/Component.h"
#include "Core/Archive/MemoryArchive.h"
#include "Object/Object.h"
#include "World/Level.h"
#include "World/Node.h"

#include <algorithm>

// 새 편집 작업을 시작하고 중첩 작업은 최상위 Transaction에 합친다.
void FEditorTransaction::Begin(FName Description)
{
	check(!bApplying);
	if (ActiveDepth++ > 0)
	{
		return;
	}

	check(RemovedTransactions.empty());
	if (NextTransactionIndex < History.size())
	{
		RemovedTransactions.reserve(History.size() - NextTransactionIndex);
		for (SIZE_T Index = NextTransactionIndex; Index < History.size(); ++Index)
		{
			RemovedTransactions.push_back(std::move(History[Index]));
		}
		History.erase(History.begin() + NextTransactionIndex, History.end());
	}
	History.push_back(FTransaction{ std::move(Description) });
}

// 최상위 편집 작업을 확정하고 변경이 없는 기록과 빈 Transaction을 제거한다.
void FEditorTransaction::End()
{
	check(ActiveDepth > 0 && !bApplying);
	if (--ActiveDepth > 0) // 바깥 Transaction이 아직 진행 중이라면, 마지막 End()에서 확정한다.
	{
		return;
	}
	FinalizeTransaction();

	if (History.back().Records.empty())
	{
		History.pop_back();
		for (FTransaction& Transaction : RemovedTransactions)
		{
			History.push_back(std::move(Transaction));
		}
		RemovedTransactions.clear();
		return;
	}
	RemovedTransactions.clear();

	NextTransactionIndex = History.size();
	if (History.size() > MaxTransactionCount)
	{
		const SIZE_T RemoveCount = History.size() - MaxTransactionCount;
		History.erase(History.begin(), History.begin() + RemoveCount);
		NextTransactionIndex -= RemoveCount;
	}
	DestroyDetachedNodes();
}

// 진행 중인 최상위 작업을 변경 전 상태로 복원하고 기록을 제거한다.
void FEditorTransaction::Cancel()
{
	check(ActiveDepth > 0 && !bApplying && !History.empty());
	ActiveDepth = 0;

	FinalizeTransaction();

	Apply(History.back(), true);
	History.pop_back();
	for (FTransaction& Transaction : RemovedTransactions)
	{
		History.push_back(std::move(Transaction));
	}
	RemovedTransactions.clear();
	DestroyDetachedNodes();
}

// 현재 객체 상태를 Transaction의 변경 전 상태로 한 번만 저장한다.
void FEditorTransaction::SaveObject(UObject& Object)
{
	SaveObject(Object, SerializeObject(Object));
}

// 호출자가 변경 전에 확보한 객체 상태를 Transaction에 한 번만 저장한다.
void FEditorTransaction::SaveObject(UObject& Object, TArray<uint8> BeforeState)
{
	check(IsActive() && !bApplying && !History.empty());
	FTransaction& Transaction = History.back();
	for (const FTransactionRecord& Variant : Transaction.Records)
	{
		if (const auto* Record = std::get_if<FObjectTransactionRecord>(&Variant); Record && Record->Object == &Object)
		{
			return;
		}
	}
	Transaction.Records.emplace_back(FObjectTransactionRecord{ &Object, std::move(BeforeState), {} });
}

// 이동할 Node들의 계층 상태를 변경 전에 한 Transaction으로 저장한다.
void FEditorTransaction::SaveHierarchy(const TArray<UNode*>& Nodes)
{
	check(IsActive() && !bApplying);
	FHierarchyMoveTransactionRecord Record;
	Record.Nodes = Nodes;
	Record.BeforeStates.reserve(Nodes.size());
	for (UNode* Node : Nodes)
	{
		check(Node && Node->IsInLevel());
		Record.BeforeStates.push_back(CaptureHierarchyMove(*Node));
	}
	History.back().Records.emplace_back(std::move(Record));
}

// 이미 Level에 연결된 새 Node를 Undo 시 분리할 수 있도록 생성 기록을 추가한다.
void FEditorTransaction::TrackNode(UNode& Node)
{
	check(IsActive() && Node.IsInLevel());
	const FNodeHierarchyState State = CaptureHierarchy(Node);
	History.back().Records.emplace_back(FNodeAttachmentRecord{ &Node.GetLevel(), &Node, State.Placement, EAttachmentChange::Added });
}

// Node 계층을 파괴하지 않고 Level에서 분리하여 Undo 가능한 삭제 기록을 추가한다.
void FEditorTransaction::DeleteNode(UNode& Node)
{
	check(IsActive() && Node.IsInLevel());
	const FNodeHierarchyState State = CaptureHierarchy(Node);
	ULevel& Level = Node.GetLevel();
	History.back().Records.emplace_back(FNodeAttachmentRecord{ &Level, &Node, State.Placement, EAttachmentChange::Removed });
	Level.DetachNode(Node);
	DetachedNodes.emplace(&Node);
}

// 이미 Node에 연결된 새 Component를 Undo 시 분리할 수 있도록 생성 기록을 추가한다.
void FEditorTransaction::TrackComponent(UComponent& Component)
{
	check(IsActive() && Component.IsOwned());
	UNode& Node = Component.GetOwner();
	const auto Iterator = std::find_if(Node.GetComponents().begin(), Node.GetComponents().end(), [&Component](const TObjectPtr<UComponent>& Candidate)
	{
		return Candidate.Get() == &Component;
	});
	check(Iterator != Node.GetComponents().end());
	History.back().Records.emplace_back(FComponentAttachmentRecord{
		&Node, &Component, static_cast<SIZE_T>(Iterator - Node.GetComponents().begin()), EAttachmentChange::Added });
}

// Component를 파괴하지 않고 Node에서 분리하여 Undo 가능한 삭제 기록을 추가한다.
void FEditorTransaction::DeleteComponent(UComponent& Component)
{
	check(IsActive() && Component.IsOwned());
	UNode& Node = Component.GetOwner();
	const SIZE_T ComponentIndex = Node.DetachComponent(Component);
	History.back().Records.emplace_back(FComponentAttachmentRecord{ &Node, &Component, ComponentIndex, EAttachmentChange::Removed });
	DetachedComponents.emplace(&Component);
}

// 직전 Transaction을 역방향으로 적용한다.
bool FEditorTransaction::Undo()
{
	if (IsActive() || bApplying || NextTransactionIndex == 0)
	{
		return false;
	}
	Apply(History[--NextTransactionIndex], true);
	return true;
}

// 다음 Transaction을 정방향으로 적용한다.
bool FEditorTransaction::Redo()
{
	if (IsActive() || bApplying || NextTransactionIndex >= History.size())
	{
		return false;
	}
	Apply(History[NextTransactionIndex++], false);
	return true;
}

// 편집 기록을 비우고 History만 소유하던 분리 Node를 실제로 파괴한다.
void FEditorTransaction::Reset()
{
	check(!bApplying);
	if (IsActive())
	{
		Cancel();
	}
	History.clear();
	RemovedTransactions.clear();
	NextTransactionIndex = 0;
	DestroyDetachedNodes();
}

// 객체의 비 Transient 리플렉션 프로퍼티를 메모리 Byte 배열로 저장한다.
TArray<uint8> FEditorTransaction::SerializeObject(UObject& Object)
{
	TArray<uint8> State;
	FMemoryWriter Writer(State);
	Object.Serialize(Writer);
	panic(!Writer.HasError());
	return State;
}

// Node의 직렬화되지 않는 계층 상태를 별도 값으로 캡처한다.
FNodeHierarchyState FEditorTransaction::CaptureHierarchy(UNode& Node)
{
	UTransformComponent& Transform = Node.GetTransform();
	return FNodeHierarchyState{
		FNodePlacement{ Transform.GetParent() ? &Transform.GetParent()->GetOwner() : nullptr, Transform.GetSiblingIndex() },
		Transform.GetRelativeTransform() };
}

// Node의 계층 상태와 현재 위치 바로 앞의 형제를 함께 캡처한다.
FHierarchyMoveState FEditorTransaction::CaptureHierarchyMove(UNode& Node)
{
	UTransformComponent& Transform = Node.GetTransform();
	UNode* PreviousSibling = nullptr;
	if (const SIZE_T SiblingIndex = Transform.GetSiblingIndex(); SiblingIndex > 0)
	{
		PreviousSibling = Transform.GetParent() ? &Transform.GetParent()->GetChildren()[SiblingIndex - 1]->GetOwner() :
			Node.GetLevel().GetRootNodes()[SiblingIndex - 1];
	}
	return FHierarchyMoveState{ CaptureHierarchy(Node), PreviousSibling };
}

// 지정 방향의 객체 상태를 역직렬화한다.
void FEditorTransaction::RestoreObject(FObjectTransactionRecord& Record, bool bUndo)
{
	check(Record.Object);
	TArray<uint8>& State = bUndo ? Record.BeforeState : Record.AfterState;
	FMemoryReader Reader(State);
	Record.Object->Serialize(Reader);
	panic(!Reader.HasError());
}

// 목표 형제 순서대로 앞에서부터 배치하여 이동한 Node의 부모와 상대 Transform을 복원한다.
void FEditorTransaction::RestoreHierarchy(FHierarchyMoveTransactionRecord& Record, bool bUndo)
{
	const TArray<FHierarchyMoveState>& States = bUndo ? Record.BeforeStates : Record.AfterStates;
	TArray<SIZE_T> Indices;
	Indices.reserve(Record.Nodes.size());
	for (SIZE_T Index = 0; Index < Record.Nodes.size(); ++Index)
	{
		Indices.push_back(Index);
	}
	std::sort(Indices.begin(), Indices.end(), [&States](SIZE_T Left, SIZE_T Right)
	{
		return States[Left].Hierarchy.Placement.SiblingIndex < States[Right].Hierarchy.Placement.SiblingIndex;
	});
	for (SIZE_T Index : Indices)
	{
		const FHierarchyMoveState& State = States[Index];
		UTransformComponent* Parent = State.Hierarchy.Placement.Parent ? &State.Hierarchy.Placement.Parent->GetTransform() : nullptr;
		UTransformComponent& Transform = Record.Nodes[Index]->GetTransform();
		check(!State.PreviousSibling || State.PreviousSibling->GetTransform().GetParent() == Parent);
		SIZE_T SiblingIndex = State.PreviousSibling ? State.PreviousSibling->GetTransform().GetSiblingIndex() + 1 : 0;
		if (Transform.GetParent() == Parent && Transform.GetSiblingIndex() < SiblingIndex)
		{
			--SiblingIndex;
		}
		panic(Transform.SetParentRelative(Parent, SiblingIndex));
		Transform.SetRelativeTransform(State.Hierarchy.RelativeTransform);
	}
}

// 지정 방향에 따라 Node 계층을 Level에 다시 연결하거나 파괴하지 않고 분리한다.
void FEditorTransaction::RestoreNodeAttachment(FNodeAttachmentRecord& Record, bool bUndo)
{
	check(Record.Level && Record.Node);
	const bool bShouldBeAttached = (Record.Change == EAttachmentChange::Added) != bUndo;
	if (bShouldBeAttached == Record.Node->IsInLevel())
	{
		return;
	}
	if (bShouldBeAttached)
	{
		Record.Level->AttachNode(*Record.Node, Record.Placement.Parent, Record.Placement.SiblingIndex);
		DetachedNodes.erase(Record.Node);
	}
	else
	{
		Record.Level->DetachNode(*Record.Node);
		DetachedNodes.emplace(Record.Node);
	}
}

// 지정 방향에 따라 Component를 원래 Node에 연결하거나 파괴하지 않고 분리한다.
void FEditorTransaction::RestoreComponentAttachment(FComponentAttachmentRecord& Record, bool bUndo)
{
	check(Record.Owner && Record.Component);
	const bool bShouldBeAttached = (Record.Change == EAttachmentChange::Added) != bUndo;
	if (bShouldBeAttached == Record.Component->IsOwned())
	{
		return;
	}
	if (bShouldBeAttached)
	{
		Record.Owner->AttachComponent(*Record.Component, Record.Index);
		DetachedComponents.erase(Record.Component);
	}
	else
	{
		Record.Owner->DetachComponent(*Record.Component);
		DetachedComponents.emplace(Record.Component);
	}
}

// Transaction 레코드를 Undo는 역순, Redo는 정순으로 적용한다.
void FEditorTransaction::Apply(FTransaction& Transaction, bool bUndo)
{
	check(!bApplying);
	bApplying = true;
	TArray<UObject*> ChangedObjects;
	for (FTransactionRecord& Variant : Transaction.Records)
	{
		if (auto* Record = std::get_if<FObjectTransactionRecord>(&Variant);
		    Record && std::find(ChangedObjects.begin(), ChangedObjects.end(), Record->Object) == ChangedObjects.end())
		{
			ChangedObjects.push_back(Record->Object);
			Record->Object->PreEditUndo();
		}
	}
	const auto ApplyRecord = [this, bUndo](FTransactionRecord& Variant)
	{
		std::visit([this, bUndo](auto& Record)
		{
			using T = std::decay_t<decltype(Record)>;
			if constexpr (std::is_same_v<T, FObjectTransactionRecord>) RestoreObject(Record, bUndo);
			else if constexpr (std::is_same_v<T, FHierarchyMoveTransactionRecord>) RestoreHierarchy(Record, bUndo);
			else if constexpr (std::is_same_v<T, FNodeAttachmentRecord>) RestoreNodeAttachment(Record, bUndo);
			else RestoreComponentAttachment(Record, bUndo);
		}, Variant);
	};
	if (bUndo)
	{
		for (auto Iterator = Transaction.Records.rbegin(); Iterator != Transaction.Records.rend(); ++Iterator)
		{
			ApplyRecord(*Iterator);
		}
	}
	else
	{
		for (FTransactionRecord& Record : Transaction.Records)
		{
			ApplyRecord(Record);
		}
	}
	for (UObject* Object : ChangedObjects)
	{
		Object->PostEditUndo();
	}
	bApplying = false;
}

// 활성 Transaction의 변경 후 상태를 기록하고 동일한 객체 상태는 제거한다.
void FEditorTransaction::FinalizeTransaction()
{
	check(!History.empty());
	FTransaction& Transaction = History.back();
	for (FTransactionRecord& Variant : Transaction.Records)
	{
		if (auto* Record = std::get_if<FObjectTransactionRecord>(&Variant))
		{
			Record->AfterState = SerializeObject(*Record->Object);
		}
		else if (auto* Record = std::get_if<FHierarchyMoveTransactionRecord>(&Variant))
		{
			Record->AfterStates.reserve(Record->Nodes.size());
			for (UNode* Node : Record->Nodes)
			{
				Record->AfterStates.push_back(CaptureHierarchyMove(*Node));
			}
		}
	}
	std::erase_if(Transaction.Records, [](const FTransactionRecord& Variant)
	{
		if (const auto* Record = std::get_if<FObjectTransactionRecord>(&Variant))
		{
			return Record->BeforeState == Record->AfterState;
		}
		if (const auto* Record = std::get_if<FHierarchyMoveTransactionRecord>(&Variant))
		{
			for (SIZE_T Index = 0; Index < Record->Nodes.size(); ++Index)
			{
				const FHierarchyMoveState& Before = Record->BeforeStates[Index];
				const FHierarchyMoveState& After = Record->AfterStates[Index];
				if (Before.Hierarchy.Placement.Parent != After.Hierarchy.Placement.Parent ||
					Before.Hierarchy.Placement.SiblingIndex != After.Hierarchy.Placement.SiblingIndex ||
					!Before.Hierarchy.RelativeTransform.ToMatrix().Equals(After.Hierarchy.RelativeTransform.ToMatrix()))
				{
					return false;
				}
			}
			return true;
		}
		return false;
	});
}

// Transaction에 더 이상 보존되지 않는 분리 Component와 Node를 실제로 파괴한다.
void FEditorTransaction::DestroyDetachedNodes()
{
	TSet<UNode*> ReferencedNodes;
	TSet<UComponent*> ReferencedComponents;
	for (const FTransaction& Transaction : History)
	{
		for (const FTransactionRecord& Variant : Transaction.Records)
		{
			if (const auto* Record = std::get_if<FNodeAttachmentRecord>(&Variant))
			{
				ReferencedNodes.emplace(Record->Node);
			}
			else if (const auto* Record = std::get_if<FComponentAttachmentRecord>(&Variant))
			{
				ReferencedComponents.emplace(Record->Component);
			}
		}
	}
	for (auto Iterator = DetachedComponents.begin(); Iterator != DetachedComponents.end();)
	{
		UComponent* Component = *Iterator;
		if (ReferencedComponents.contains(Component))
		{
			++Iterator;
			continue;
		}
		Iterator = DetachedComponents.erase(Iterator);
		static_cast<UNode*>(Component->GetOuter())->DestroyDetachedComponent(*Component);
	}
	for (auto Iterator = DetachedNodes.begin(); Iterator != DetachedNodes.end();)
	{
		UNode* DetachedNode = *Iterator;
		if (ReferencedNodes.contains(DetachedNode))
		{
			++Iterator;
			continue;
		}
		Iterator = DetachedNodes.erase(Iterator);
		DetachedNode->GetLevel().DestroyDetachedNode(*DetachedNode);
	}
}
