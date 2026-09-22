#include "Editor/EditorSelection.h"

#include "World/Node.h"

#include <algorithm>

// 기존 선택을 해제하고 단일 Node를 활성 선택으로 지정한다.
void FEditorSelection::Select(UNode* Node)
{
	if (SelectedNodes.size() == 1 && SelectedNode == Node)
	{
		return;
	}
	Deselect();
	if (Node)
	{
		SelectedNodes.push_back(Node);
		SelectedNode = Node;
		Node->SetSelected(true);
	}
}

// 기존 선택을 입력 Node 집합으로 교체하고 활성 Node를 별도로 지정한다.
void FEditorSelection::Select(const TArray<UNode*>& Nodes, UNode* ActiveNode)
{
	Deselect();
	for (UNode* Node : Nodes)
	{
		if (!Node || IsSelected(Node))
		{
			continue;
		}
		SelectedNodes.push_back(Node);
		Node->SetSelected(true);
	}
	SelectedNode = IsSelected(ActiveNode) ? ActiveNode : (SelectedNodes.empty() ? nullptr : SelectedNodes.back());
}

// 기존 선택을 유지하면서 Node를 선택 집합과 활성 선택에 추가한다.
void FEditorSelection::Add(UNode& Node)
{
	if (!IsSelected(&Node))
	{
		SelectedNodes.push_back(&Node);
		Node.SetSelected(true);
	}
	SelectedNode = &Node;
}

// Node의 선택 여부를 반전하고 남은 선택 중 마지막 Node를 활성화한다.
void FEditorSelection::Toggle(UNode& Node)
{
	const auto Iterator = std::find(SelectedNodes.begin(), SelectedNodes.end(), &Node);
	if (Iterator == SelectedNodes.end())
	{
		Add(Node);
		return;
	}

	Node.SetSelected(false);
	SelectedNodes.erase(Iterator);
	SelectedNode = SelectedNodes.empty() ? nullptr : SelectedNodes.back();
}

// 모든 Node의 선택 상태를 해제한다.
void FEditorSelection::Deselect()
{
	for (UNode* Node : SelectedNodes)
	{
		Node->SetSelected(false);
	}
	SelectedNodes.clear();
	SelectedNode = nullptr;
}

// Node가 현재 선택 집합에 포함되었는지 반환한다.
bool FEditorSelection::IsSelected(const UNode* Node) const
{
	return std::find(SelectedNodes.begin(), SelectedNodes.end(), Node) != SelectedNodes.end();
}
