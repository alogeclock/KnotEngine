#pragma once

#include "Core/CoreTypes.h"

class UNode;

// Editor 전역에서 공유하는 현재 선택 데이터이며 UEditorEngine이 유일하게 소유한다.
struct FEditorSelection
{
	void Select(UNode* Node);
	void Select(const TArray<UNode*>& Nodes, UNode* ActiveNode);
	void Add(UNode& Node);
	void Toggle(UNode& Node);
	void Deselect();
	bool IsSelected(const UNode* Node) const;
	const TArray<UNode*>& GetSelectedNodes() const { return SelectedNodes; }

	UNode* SelectedNode = nullptr;

private:
	TArray<UNode*> SelectedNodes;
};
