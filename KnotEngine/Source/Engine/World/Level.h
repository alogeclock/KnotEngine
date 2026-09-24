#pragma once

#include "World/Node.h"

class UComponent;
class FHierarchyPanel;

// World에 속한 Node의 저장 및 관리 단위.
// 공간상의 부모-자식 관계와 관계없이 소속된 모든 Node를 평탄한 배열로 소유하며,
// World의 플레이 생명주기와 Tick 요청을 각 Node에 전달한다.
UCLASS()
class ENGINE_API ULevel : public UObject
{
	GENERATED_CLASS(ULevel, UObject)

public:
	ULevel() = default;
	~ULevel() override;
	
	void PostInitProperties() override;
	void PostDuplicate() override;
	
	UWorld& GetWorld() const;

	UNode& CreateNode(FName Name);
	UNode& CreateNode(const UClass& NodeClass, FName Name);
	TArray<UNode*> DuplicateNodes(const TArray<UNode*>& SourceNodes);
	void RemoveNode(UNode& Node);

	void DetachNode(UNode& Node);
	void AttachNode(UNode& Node, UNode* Parent, SIZE_T SiblingIndex);
	void DestroyDetachedNode(UNode& Node);
	bool ReparentNodesAbsolute(const TArray<UNode*>& NodesToMove, UNode* NewParent, SIZE_T SiblingIndex);

	const TArray<TObjectPtr<UNode>>& GetNodes() const { return Nodes; }
	const TArray<UNode*>& GetRootNodes() const { return RootNodes; }

	void BeginPlay();
	void EndPlay();

	void Tick(float DeltaTime);

private:
	friend class UComponent;
	friend class FHierarchyPanel;
	friend class UTransformComponent;
	friend class UWorld;

	void InsertRootNode(UNode& Node, SIZE_T SiblingIndex);
	void RemoveRootNode(UNode& Node);
	bool SetRootSiblingIndex(UNode& Node, SIZE_T SiblingIndex);

	void TransferNodes(ULevel& Destination);

	void RegisterTickComponent(UComponent& Component);
	void UnregisterTickComponent(UComponent& Component);

	UPROPERTY(NoEdit, Transient) TObjectPtr<UWorld> OwningWorld;
	UPROPERTY(NoEdit, Transient) TArray<TObjectPtr<UNode>> Nodes;
	TArray<UNode*> RootNodes; // Level이 소유하는 Root Node의 Hierarchy 순서만 관리하는 비소유 인덱스
	TArray<UComponent*> TickComponents; // 활성화된 Tick Component의 비소유 밀집 배열
};
