#include "Editor/Panel/HierarchyPanel.h"

#include "Component/Component.h"
#include "Editor/EditorSelection.h"
#include "Object/Class.h"
#include "Object/Reflection/ReflectionRegistry.h"
#include "World/Level.h"
#include "World/Node.h"
#include "World/World.h"

#include <imgui.h>

#include <limits>

UNode& FHierarchyPanel::CreateNode(UWorld& World, const FString& BaseName)
{
	return World.GetPersistentLevel().CreateNode(World.GetNodeName(BaseName));
}

bool FHierarchyPanel::DrawNode(UWorld& World, FEditorSelection& Selection, const UClass& ComponentClass)
{
	const FString& DisplayName = ComponentClass.GetMetadata().GetDisplayName();
	if (!ImGui::MenuItem(DisplayName.c_str()))
	{
		return false;
	}

	UNode& Node = CreateNode(World, DisplayName);
	Node.AddComponent(ComponentClass);
	Selection.Select(&Node);
	return true;
}

void FHierarchyPanel::DrawLevel(UWorld& World, ULevel& Level, SIZE_T LevelIndex, FEditorSelection& Selection)
{
	ImGui::PushID(&Level);
	const FString LevelLabel = &Level == &World.GetPersistentLevel() ? "Persistent Level" : "Level " + std::to_string(LevelIndex);
	if (ImGui::TreeNodeEx(LevelLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
	{
		const TArray<TObjectPtr<UNode>>& Nodes = Level.GetNodes();
		check(Nodes.size() <= static_cast<SIZE_T>((std::numeric_limits<int>::max)()));
		
		// 화면에 보이는 객체의 텍스트만 렌더링하도록 한다.
		ImGuiListClipper Clipper;
		Clipper.Begin(static_cast<int>(Nodes.size()));
		while (Clipper.Step())
		{
			for (int NodeIndex = Clipper.DisplayStart; NodeIndex < Clipper.DisplayEnd; ++NodeIndex)
			{
				UNode* Node = Nodes[static_cast<SIZE_T>(NodeIndex)].Get();
				if (!Node)
				{
					ImGui::Dummy({ 0.0f, ImGui::GetTextLineHeightWithSpacing() });
					continue;
				}

				ImGui::PushID(Node);
				const FString NodeName = Node->GetName().ToString();
				const bool bSelected = Selection.SelectedNode == Node;
				if (ImGui::Selectable(NodeName.c_str(), bSelected))
				{
					Selection.Select(Node);
				}
				if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
				{
					Selection.Select(Node);
				}
				ImGui::PopID();
			}
		}
		ImGui::TreePop();
	}
	ImGui::PopID();
}

// Hierarchy 어디에서나 Node 생성과 현재 선택 Node 제거 메뉴를 표시한다.
bool FHierarchyPanel::DrawContextMenu(UWorld& World, FEditorSelection& Selection)
{
	if (!ImGui::BeginPopupContextWindow("##HierarchyContextMenu", ImGuiPopupFlags_MouseButtonRight))
	{
		return false;
	}

	if (ImGui::BeginMenu("Add Node"))
	{
		// Empty는 TransformComponent만 가진 기본 Node를 생성한다.
		if (ImGui::MenuItem("Empty"))
		{
			Selection.Select(&CreateNode(World, "Node"));
		}
		// Registry가 등록 시점에 정렬한 EditorSpawnable Class 목록을 참조한다.
		check(GReflectionRegistry);
		const TArray<const UClass*>& ComponentClasses = GReflectionRegistry->GetEditorSpawnableClasses();

		// 정렬된 Component를 같은 Category 단위로 묶는다.
		bool bNodeCreated = false;
		for (SIZE_T FirstClass = 0; FirstClass < ComponentClasses.size() && !bNodeCreated;)
		{
			while (FirstClass < ComponentClasses.size() && !ComponentClasses[FirstClass]->IsChildOf(UComponent::StaticClass()))
			{
				++FirstClass;
			}
			if (FirstClass == ComponentClasses.size())
			{
				break;
			}

			const FString& Category = ComponentClasses[FirstClass]->GetMetadata().GetCategory();
			SIZE_T LastClass = FirstClass + 1;
			while (LastClass < ComponentClasses.size() && ComponentClasses[LastClass]->GetMetadata().GetCategory() == Category)
			{
				++LastClass;
			}

			const FString MenuName = Category.empty() ? "Other" : Category;
			if (ImGui::BeginMenu(MenuName.c_str()))
			{
				for (SIZE_T ClassIndex = FirstClass; ClassIndex < LastClass; ++ClassIndex)
				{
					if (!ComponentClasses[ClassIndex]->IsChildOf(UComponent::StaticClass()))
					{
						continue;
					}
					if (DrawNode(World, Selection, *ComponentClasses[ClassIndex]))
					{
						bNodeCreated = true;
						break;
					}
				}
				ImGui::EndMenu();
			}
			FirstClass = LastClass;
		}
		ImGui::EndMenu();
	}

	const bool bRemoveNode = ImGui::MenuItem("Remove Node", nullptr, false, Selection.SelectedNode != nullptr);
	ImGui::EndPopup();
	return bRemoveNode;
}

void FHierarchyPanel::Draw(UWorld& World, FEditorSelection& Selection)
{
	if (!ImGui::Begin("Hierarchy"))
	{
		ImGui::End();
		return;
	}

	const TArray<TObjectPtr<ULevel>>& Levels = World.GetLevels();
	for (SIZE_T LevelIndex = 0; LevelIndex < Levels.size(); ++LevelIndex)
	{
		ULevel* Level = Levels[LevelIndex].Get();
		if (!Level)
		{
			continue;
		}
		DrawLevel(World, *Level, LevelIndex, Selection);
	}
	if (DrawContextMenu(World, Selection))
	{
		UNode* NodeToRemove = Selection.SelectedNode;
		Selection.Deselect();
		NodeToRemove->GetLevel().RemoveNode(*NodeToRemove);
	}
	ImGui::End();
}
