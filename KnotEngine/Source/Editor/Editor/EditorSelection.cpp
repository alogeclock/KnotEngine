#include "Editor/EditorSelection.h"

#include "World/Node.h"

// 선택이 변경된 Node에만 상태를 전달하여 소유 Primitive Proxy의 선택 Cache를 갱신한다.
void FEditorSelection::SelectNode(UNode* Node)
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
	if (SelectedNode)
	{
		SelectedNode->SetSelected(true);
	}
}
