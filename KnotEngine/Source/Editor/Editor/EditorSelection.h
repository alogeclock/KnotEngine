#pragma once

class UNode;

// Editor 전역에서 공유하는 현재 선택 데이터이며 UEditorEngine이 유일하게 소유한다.
struct FEditorSelection
{
	void Select(UNode* Node);
	void Deselect();

	UNode* SelectedNode = nullptr;
};
