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
	uint64 GetGeneration() const { return Generation; }

	UNode* SelectedNode = nullptr;

private:
	TArray<UNode*> SelectedNodes;
	uint64 Generation = 0; // 선택 함수 호출 시 증가하며, Ray 요청 뒤 Hierarchy·박스 선택 등으로 다시 선택했는지 확인해 이전 결과를 버린다.
};
