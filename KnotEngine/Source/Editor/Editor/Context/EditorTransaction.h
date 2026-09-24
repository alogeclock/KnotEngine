#pragma once

#include "Editor/Context/Transaction.h"

class FEditorTransaction final
{
public:
	void Begin(FName Description);
	void End();
	void Cancel();

	void SaveObject(UObject& Object);
	void SaveObject(UObject& Object, TArray<uint8> BeforeState);
	void SaveHierarchy(UNode& Node);

	void TrackNode(UNode& Node);
	void DeleteNode(UNode& Node);

	void TrackComponent(UComponent& Component);
	void DeleteComponent(UComponent& Component);

	bool Undo();
	bool Redo();
	void Reset();

	bool IsActive() const { return ActiveDepth > 0; }
	bool IsApplying() const { return bApplying; }

private:
	static constexpr SIZE_T MaxTransactionCount = 128;

	static TArray<uint8> SerializeObject(UObject& Object);
	static FNodeHierarchyState CaptureHierarchy(UNode& Node);
	static void RestoreObject(FObjectTransactionRecord& Record, bool bUndo);
	static void RestoreHierarchy(FHierarchyTransactionRecord& Record, bool bUndo);
	void RestoreNodeAttachment(FNodeAttachmentRecord& Record, bool bUndo);
	void RestoreComponentAttachment(FComponentAttachmentRecord& Record, bool bUndo);

	void Apply(FTransaction& Transaction, bool bUndo);
	void FinalizeTransaction();
	void DestroyDetachedNodes();

	TArray<FTransaction> History; // Transaction History 기록
	TArray<FTransaction> RemovedTransactions; // Undo 후 새 편집을 시작할 때, Redo 기록을 잠시 보관하는 배열

	SIZE_T NextTransactionIndex = 0;
	uint32 ActiveDepth = 0;
	bool bApplying = false;

	TSet<UNode*> DetachedNodes; // Level에서 분리됐지만, Undo/Redo를 위해 아직 파괴하지 않은 노드
	TSet<UComponent*> DetachedComponents; // Node에서 분리됐지만, Undo/Redo를 위해 아직 파괴하지 않은 컴포넌트
};
