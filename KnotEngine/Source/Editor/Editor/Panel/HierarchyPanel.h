#pragma once

#include "Core/CoreTypes.h"

struct FEditorSelection;
class FInputSnapshot;
class FTransactionManager;
class UClass;
class ULevel;
class UNode;
class UWorld;

class FHierarchyPanel
{
public:
	explicit FHierarchyPanel(FTransactionManager& InTransactionManager) : TransactionManager(InTransactionManager) {}
	void Draw(UWorld& World, FEditorSelection& Selection, const FInputSnapshot& InputSnapshot);

private:
	enum class ENodeDropPosition : uint8
	{
		Before,
		Into,
		After,
		Root,
	};

	struct FVisibleNode
	{
		UNode* Node = nullptr;
		uint32 Depth = 0;
		bool bHasChildren = false;
	};

	struct FPendingDropNode
	{
		UNode* DraggedNode = nullptr;
		UNode* TargetNode = nullptr;
		ULevel* TargetLevel = nullptr;
		ENodeDropPosition Position = ENodeDropPosition::Into;
		bool bValid = false;
	};

	void RemoveSelectedNodes(FEditorSelection& Selection);

	void BuildVisibleNodes(UNode& Node, uint32 Depth);
	void SelectVisibleNode(UNode& Node, FEditorSelection& Selection, const FInputSnapshot& InputSnapshot);
	void DrawVisibleNode(const FVisibleNode& VisibleNode, FEditorSelection& Selection, const FInputSnapshot& InputSnapshot);
	void DrawLevel(UWorld& World, ULevel& Level, SIZE_T LevelIndex, FEditorSelection& Selection, const FInputSnapshot& InputSnapshot);
	bool DrawContextMenu(UWorld& World, FEditorSelection& Selection);

	void ApplyPendingDrop();

	FTransactionManager& TransactionManager;
	TArray<FVisibleNode> VisibleNodes;
	UNode* SelectionAnchor = nullptr;
	FPendingDropNode PendingDrop;

	TSet<uint32> CollapsedNodeUUIDs;
};
