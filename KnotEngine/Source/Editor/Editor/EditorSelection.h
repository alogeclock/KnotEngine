#pragma once

class UNode;

// Editor 전역에서 공유하는 현재 선택 데이터이며 UEditorEngine이 유일하게 소유한다.
struct FEditorSelection
{
	void SelectNode(UNode* Node);
	void Clear() { SelectNode(nullptr); }

	UNode* SelectedNode = nullptr;
};
