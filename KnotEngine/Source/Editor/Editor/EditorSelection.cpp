#include "Editor/EditorSelection.h"

#include "World/Node.h"

// 선택이 변경된 Node에만 상태를 전달하여 소유 Primitive Proxy의 선택 Cache를 갱신한다.
void FEditorSelection::Select(UNode* Node)
{
	if (SelectedNode == Node)
	{
		return;
	}

	if (SelectedNode)
	{
		SelectedNode->SetSelected(false);
	}

	SelectedNode = Node;
	if (SelectedNode) // nullptr rejection
	{
		SelectedNode->SetSelected(true);
	}
}

// Editor Selection의 선택 상태를 해제하고 선택된 노드에도 전달한다.
void FEditorSelection::Deselect()
 {
	Select(nullptr);
}
