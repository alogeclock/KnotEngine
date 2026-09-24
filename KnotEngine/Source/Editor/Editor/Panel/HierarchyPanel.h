#pragma once

#include "Core/CoreTypes.h"

struct FEditorSelection;
class FInputSnapshot;
class FEditorTransaction;

class UClass;

class ULevel;
class UNode;
class UWorld;

class FHierarchyPanel
{
public:
	FHierarchyPanel();

	void Draw(UWorld& World, FEditorSelection& Selection, const FInputSnapshot& InputSnapshot);

	void RemoveSelectedNodes(FEditorSelection& Selection);

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

	void BuildVisibleNodes(UNode& Node, uint32 Depth);
	void SelectVisibleNode(UNode& Node, FEditorSelection& Selection, const FInputSnapshot& InputSnapshot);
	void DrawVisibleNode(const FVisibleNode& VisibleNode, FEditorSelection& Selection, const FInputSnapshot& InputSnapshot);
	void DrawLevel(UWorld& World, ULevel& Level, SIZE_T LevelIndex, FEditorSelection& Selection, const FInputSnapshot& InputSnapshot);
	bool DrawContextMenu(UWorld& World, FEditorSelection& Selection);

	void ApplyPendingDrop(const FEditorSelection& Selection);

	FEditorTransaction& EditorTransaction;
	TArray<FVisibleNode> VisibleNodes;
	UNode* SelectionAnchor = nullptr;
	UNode* PendingSelectionClick = nullptr;
	FPendingDropNode PendingDrop;

	TSet<uint32> CollapsedNodeUUIDs;
};
