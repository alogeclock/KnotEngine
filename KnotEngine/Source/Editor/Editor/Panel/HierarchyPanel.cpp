#include "Editor/Panel/HierarchyPanel.h"

#include "Core/Input/InputSnapshot.h"
#include "Editor/Context/EditorSelection.h"
#include "Editor/Widget/NodeCreationMenu.h"
#include "Editor/Context/EditorTransaction.h"
#include "Component/TransformComponent.h"
#include "Runtime/EditorEngine.h"
#include "World/Level.h"
#include "World/Node.h"
#include "World/World.h"

#include <imgui.h>

#include <algorithm>
#include <limits>

FHierarchyPanel::FHierarchyPanel()
    : EditorTransaction(GetEditor().GetEditorTransaction())
{
}

// 선택된 자손을 중복 제거한 뒤 선택 계층의 최상위 Node부터 제거한다.
void FHierarchyPanel::RemoveSelectedNodes(FEditorSelection& Selection)
{
	const TArray<UNode*>& SelectedNodes = Selection.GetSelectedNodes();
	TSet<UNode*> SelectedLookup(SelectedNodes.begin(), SelectedNodes.end());
	TArray<UNode*> RemovalRoots;
	for (UNode* Node : SelectedNodes)
	{
		bool bHasAncestor = false;
		for (UTransformComponent* Parent = Node->GetParent(); Parent; Parent = Parent->GetParent())
		{
			if (SelectedLookup.contains(&Parent->GetOwner()))
			{
				bHasAncestor = true;
				break;
			}
		}
		if (!bHasAncestor)
		{
			RemovalRoots.push_back(Node);
		}
	}

	EditorTransaction.Begin(FName(RemovalRoots.size() > 1 ? "Remove Nodes" : "Remove Node"));
	Selection.Deselect();
	for (UNode* Node : RemovalRoots)
	{
		EditorTransaction.DeleteNode(*Node);
	}
	EditorTransaction.End();
}

// 펼쳐진 Node와 자손만 화면 표시 순서의 평탄 목록에 추가한다.
void FHierarchyPanel::BuildVisibleNodes(UNode& Node, uint32 Depth)
{
	const TArray<TObjectPtr<UTransformComponent>>& Children = Node.GetChildren();
	VisibleNodes.push_back(FVisibleNode{ &Node, Depth, !Children.empty() });
	if (CollapsedNodeUUIDs.contains(Node.GetUUID()))
	{
		return;
	}
	for (const TObjectPtr<UTransformComponent>& Child : Children)
	{
		BuildVisibleNodes(Child->GetOwner(), Depth + 1);
	}
}

// Ctrl은 선택을 토글하고 Shift는 현재 가시 목록에서 Anchor부터 범위를 선택한다.
void FHierarchyPanel::SelectVisibleNode(UNode& Node, FEditorSelection& Selection, const FInputSnapshot& InputSnapshot)
{
	const EModifierKeyMask Modifiers = InputSnapshot.GetModifiers();
	const bool bControlDown = HasModifierKey(Modifiers, EModifierKeyMask::Control);
	const bool bShiftDown = HasModifierKey(Modifiers, EModifierKeyMask::Shift);
	if (bShiftDown && SelectionAnchor)
	{
		const auto Anchor = std::find_if(VisibleNodes.begin(), VisibleNodes.end(), [this](const FVisibleNode& VisibleNode)
		{
			return VisibleNode.Node == SelectionAnchor;
		});
		const auto Target = std::find_if(VisibleNodes.begin(), VisibleNodes.end(), [&Node](const FVisibleNode& VisibleNode)
		{
			return VisibleNode.Node == &Node;
		});
		if (Anchor != VisibleNodes.end() && Target != VisibleNodes.end())
		{
			TArray<UNode*> Range = bControlDown ? Selection.GetSelectedNodes() : TArray<UNode*>();
			const auto First = (std::min)(Anchor, Target);
			const auto Last = (std::max)(Anchor, Target);
			for (auto Iterator = First; Iterator <= Last; ++Iterator)
			{
				if (std::find(Range.begin(), Range.end(), Iterator->Node) == Range.end())
				{
					Range.push_back(Iterator->Node);
				}
			}
			Selection.Select(Range, &Node);
			return;
		}
	}

	if (bControlDown)
	{
		Selection.Toggle(Node);
	}
	else
	{
		Selection.Select(&Node);
	}
	SelectionAnchor = &Node;
}

// 한 가시 Node 행의 펼침, 선택과 Drag & Drop 입력을 처리한다.
void FHierarchyPanel::DrawVisibleNode(const FVisibleNode& VisibleNode, FEditorSelection& Selection, const FInputSnapshot& InputSnapshot)
{
	UNode& Node = *VisibleNode.Node;
	ImGui::PushID(&Node);
	if (VisibleNode.Depth > 0)
	{
		ImGui::Indent(static_cast<float>(VisibleNode.Depth) * ImGui::GetStyle().IndentSpacing);
	}

	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_SpanAvailWidth |
		ImGuiTreeNodeFlags_NoTreePushOnOpen |
		ImGuiTreeNodeFlags_OpenOnArrow |
		ImGuiTreeNodeFlags_OpenOnDoubleClick;
	if (Selection.IsSelected(&Node))
	{
		Flags |= ImGuiTreeNodeFlags_Selected;
	}
	if (!VisibleNode.bHasChildren)
	{
		Flags |= ImGuiTreeNodeFlags_Leaf;
	}

	const bool bExpanded = VisibleNode.bHasChildren && !CollapsedNodeUUIDs.contains(Node.GetUUID());
	ImGui::SetNextItemOpen(bExpanded, ImGuiCond_Always);
	const FString NodeName = Node.GetName().ToString();
	const bool bOpen = ImGui::TreeNodeEx("##Node", Flags, "%s", NodeName.c_str());
	const ImVec2 ItemMinimum = ImGui::GetItemRectMin();
	const ImVec2 ItemMaximum = ImGui::GetItemRectMax();
	if (VisibleNode.bHasChildren)
	{
		if (bOpen)
		{
			CollapsedNodeUUIDs.erase(Node.GetUUID());
		}
		else
		{
			CollapsedNodeUUIDs.emplace(Node.GetUUID());
		}
	}
	if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
	{
		if (Selection.IsSelected(&Node) && Selection.GetSelectedNodes().size() > 1 && InputSnapshot.GetModifiers() == EModifierKeyMask::None)
		{
			PendingSelectionClick = &Node;
		}
		else
		{
			PendingSelectionClick = nullptr;
			SelectVisibleNode(Node, Selection, InputSnapshot);
		}
	}
	if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && !Selection.IsSelected(&Node))
	{
		Selection.Select(&Node);
		SelectionAnchor = &Node;
	}

	if (ImGui::BeginDragDropSource())
	{
		PendingSelectionClick = nullptr;
		if (!Selection.IsSelected(&Node))
		{
			Selection.Select(&Node);
			SelectionAnchor = &Node;
		}
		UNode* PayloadNode = &Node;
		ImGui::SetDragDropPayload("HIERARCHY_NODE", &PayloadNode, sizeof(PayloadNode));
		ImGui::TextUnformatted(NodeName.c_str());
		ImGui::EndDragDropSource();
	}

	if (ImGui::BeginDragDropTarget())
	{
		const ImGuiDragDropFlags DropFlags =
			ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect;
		const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("HIERARCHY_NODE", DropFlags);
		if (Payload && Payload->DataSize == sizeof(UNode*))
		{
			const float ItemHeight = ItemMaximum.y - ItemMinimum.y;
			const float Position = ItemHeight > 0.0f ? (ImGui::GetMousePos().y - ItemMinimum.y) / ItemHeight : 0.5f;
			const ENodeDropPosition DropPosition = Position < 0.25f ? ENodeDropPosition::Before :
				(Position > 0.75f ? ENodeDropPosition::After : ENodeDropPosition::Into);
			const ImU32 Color = ImGui::GetColorU32(ImGuiCol_DragDropTarget);
			if (DropPosition == ENodeDropPosition::Into)
			{
				ImGui::GetForegroundDrawList()->AddRect(ItemMinimum, ItemMaximum, Color, 2.0f, 0, 2.0f);
			}
			else
			{
				const float Y = DropPosition == ENodeDropPosition::Before ? ItemMinimum.y : ItemMaximum.y;
				ImGui::GetForegroundDrawList()->AddLine(ImVec2(ItemMinimum.x, Y), ImVec2(ItemMaximum.x, Y), Color, 2.0f);
			}
			if (Payload->IsDelivery())
			{
				PendingDrop.DraggedNode = *static_cast<UNode* const*>(Payload->Data);
				PendingDrop.TargetNode = &Node;
				PendingDrop.TargetLevel = &Node.GetLevel();
				PendingDrop.Position = DropPosition;
				PendingDrop.bValid = true;
			}
		}
		ImGui::EndDragDropTarget();
	}

	if (VisibleNode.Depth > 0)
	{
		ImGui::Unindent(static_cast<float>(VisibleNode.Depth) * ImGui::GetStyle().IndentSpacing);
	}
	ImGui::PopID();
}

// Level의 Root부터 가시 계층을 만들고 화면에 걸친 행만 실제로 렌더링한다.
void FHierarchyPanel::DrawLevel(UWorld& World, ULevel& Level, SIZE_T LevelIndex, FEditorSelection& Selection, const FInputSnapshot& InputSnapshot)
{
	ImGui::PushID(&Level);
	const bool bPersistentLevel = &Level == &World.GetPersistentLevel();
	bool bLevelOpen = true;
	if (!bPersistentLevel)
	{
		const FString LevelLabel = "Level " + std::to_string(LevelIndex);
		bLevelOpen = ImGui::TreeNodeEx(LevelLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
		if (ImGui::BeginDragDropTarget())
		{
			const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("HIERARCHY_NODE", ImGuiDragDropFlags_AcceptBeforeDelivery);
			if (Payload && Payload->DataSize == sizeof(UNode*) && Payload->IsDelivery())
			{
				PendingDrop.DraggedNode = *static_cast<UNode* const*>(Payload->Data);
				PendingDrop.TargetLevel = &Level;
				PendingDrop.Position = ENodeDropPosition::Root;
				PendingDrop.bValid = true;
			}
			ImGui::EndDragDropTarget();
		}
	}
	if (bLevelOpen)
	{
		VisibleNodes.clear();
		for (UNode* RootNode : Level.RootNodes)
		{
			BuildVisibleNodes(*RootNode, 0);
		}
		check(VisibleNodes.size() <= static_cast<SIZE_T>((std::numeric_limits<int>::max)()));

		ImGuiListClipper Clipper;
		Clipper.Begin(static_cast<int>(VisibleNodes.size()));
		while (Clipper.Step())
		{
			for (int Index = Clipper.DisplayStart; Index < Clipper.DisplayEnd; ++Index)
			{
				DrawVisibleNode(VisibleNodes[static_cast<SIZE_T>(Index)], Selection, InputSnapshot);
			}
		}
		if (!bPersistentLevel)
		{
			ImGui::TreePop();
		}
	}
	ImGui::PopID();
}

// 보류한 Drop을 계층 순회가 끝난 뒤 적용하여 가시 목록과 자식 배열의 무효화를 피한다.
void FHierarchyPanel::ApplyPendingDrop(const FEditorSelection& Selection)
{
	// 유효한 Drop이며 드래그한 Node와 대상 Level이 일치하는지 확인한다.
	if (!PendingDrop.bValid || !PendingDrop.DraggedNode || !PendingDrop.TargetLevel ||
		&PendingDrop.DraggedNode->GetLevel() != PendingDrop.TargetLevel)
	{
		return;
	}

	// 선택된 자손을 제외하고 실제로 이동할 최상위 Node만 모은다.
	const TArray<UNode*>& SelectedNodes = Selection.GetSelectedNodes();
	TSet<UNode*> SelectedLookup(SelectedNodes.begin(), SelectedNodes.end());
	TArray<UNode*> DragRoots;
	DragRoots.reserve(SelectedNodes.size());
	for (UNode* Node : SelectedNodes)
	{
		if (&Node->GetLevel() != PendingDrop.TargetLevel)
		{
			return;
		}
		bool bSelectedAncestor = false;
		for (UTransformComponent* Parent = Node->GetParent(); Parent; Parent = Parent->GetParent())
		{
			if (SelectedLookup.contains(&Parent->GetOwner()))
			{
				bSelectedAncestor = true;
				break;
			}
		}
		if (!bSelectedAncestor)
		{
			DragRoots.push_back(Node);
		}
	}

	// 다중 이동은 Node별 계층 경로를 한 번만 계산해 Hierarchy 표시 순서로 정렬한다.
	if (DragRoots.size() > 1)
	{
		struct FSortedDragRoot
		{
			UNode* Node = nullptr;
			TArray<SIZE_T> Path;
		};
		TArray<FSortedDragRoot> SortedRoots;
		SortedRoots.reserve(DragRoots.size());
		for (UNode* Root : DragRoots)
		{
			FSortedDragRoot& Entry = SortedRoots.emplace_back();
			Entry.Node = Root;
			for (const UNode* Node = Root; Node; Node = Node->GetParent() ? &Node->GetParent()->GetOwner() : nullptr)
			{
				Entry.Path.push_back(Node->GetSiblingIndex());
			}
			std::reverse(Entry.Path.begin(), Entry.Path.end());
		}
		std::sort(SortedRoots.begin(), SortedRoots.end(), [](const FSortedDragRoot& Left, const FSortedDragRoot& Right)
		{
			return std::lexicographical_compare(Left.Path.begin(), Left.Path.end(), Right.Path.begin(), Right.Path.end());
		});
		DragRoots.clear();
		for (const FSortedDragRoot& Entry : SortedRoots)
		{
			DragRoots.push_back(Entry.Node);
		}
	}

	// 선택된 Node 또는 그 자손 아래로 옮겨 순환 계층이 생기는 것을 막는다.
	for (UNode* Target = PendingDrop.TargetNode; Target; Target = Target->GetParent() ? &Target->GetParent()->GetOwner() : nullptr)
	{
		if (SelectedLookup.contains(Target))
		{
			return;
		}
	}

	// Drop 위치에 따른 새 부모를 정하고 각 이동 Node의 원래 위치를 기록한다.
	UNode* DestinationParent = PendingDrop.Position == ENodeDropPosition::Into ? PendingDrop.TargetNode :
		(PendingDrop.TargetNode && PendingDrop.TargetNode->GetParent() ? &PendingDrop.TargetNode->GetParent()->GetOwner() : nullptr);
	SIZE_T NewSiblingIndex = 0;
	if (PendingDrop.Position == ENodeDropPosition::Into)
	{
		NewSiblingIndex = DestinationParent->GetChildren().size();
	}
	else if (PendingDrop.Position == ENodeDropPosition::Root)
	{
		NewSiblingIndex = PendingDrop.TargetLevel->GetRootNodes().size();
	}
	else
	{
		NewSiblingIndex = PendingDrop.TargetNode->GetSiblingIndex() + (PendingDrop.Position == ENodeDropPosition::After ? 1 : 0);
	}
	EditorTransaction.Begin(FName(DragRoots.size() > 1 ? "Reparent Nodes" : "Reparent Node"));
	EditorTransaction.SaveHierarchy(DragRoots);

	// World Transform을 유지한 채 형제 배열을 일괄 갱신하고 결과를 확정한다.
	if (PendingDrop.TargetLevel->ReparentNodesAbsolute(DragRoots, DestinationParent, NewSiblingIndex))
	{
		EditorTransaction.End();
	}
	else
	{
		EditorTransaction.Cancel();
	}
}

// Hierarchy 어디에서나 Node 생성과 현재 선택 Node 제거 메뉴를 표시한다.
bool FHierarchyPanel::DrawContextMenu(UWorld& World, FEditorSelection& Selection)
{
	if (!ImGui::BeginPopupContextWindow("##HierarchyContextMenu", ImGuiPopupFlags_MouseButtonRight))
	{
		return false;
	}

	if (UNode* CreatedNode = FNodeCreationMenu::Draw(World))
	{
		EditorTransaction.Begin(FName("Add Node"));
		EditorTransaction.TrackNode(*CreatedNode);
		EditorTransaction.End();
		Selection.Select(CreatedNode);
	}

	const bool bMultipleNodes = Selection.GetSelectedNodes().size() > 1;
	const bool bRemoveNode = ImGui::MenuItem(bMultipleNodes ? "Remove Nodes" : "Remove Node", nullptr, false, Selection.SelectedNode != nullptr);
	ImGui::EndPopup();
	return bRemoveNode;
}

void FHierarchyPanel::Draw(UWorld& World, FEditorSelection& Selection, const FInputSnapshot& InputSnapshot)
{
	if (!ImGui::Begin("Hierarchy"))
	{
		ImGui::End();
		return;
	}

	PendingDrop = FPendingDropNode();
	const TArray<TObjectPtr<ULevel>>& Levels = World.GetLevels();
	for (SIZE_T LevelIndex = 0; LevelIndex < Levels.size(); ++LevelIndex)
	{
		ULevel* Level = Levels[LevelIndex].Get();
		if (!Level)
		{
			continue;
		}
		DrawLevel(World, *Level, LevelIndex, Selection, InputSnapshot);
	}
	ApplyPendingDrop(Selection);

	if (InputSnapshot.WasMouseButtonReleased(EMouseButton::Left) && PendingSelectionClick)
	{
		Selection.Select(PendingSelectionClick);
		SelectionAnchor = PendingSelectionClick;
		PendingSelectionClick = nullptr;
	}

	if (DrawContextMenu(World, Selection))
	{
		RemoveSelectedNodes(Selection);
		SelectionAnchor = nullptr;
	}

	ImGui::End();
}
