#include "ImGui/Panels/HierarchyPanel.h"

#include "Component/Component.h"
#include "ImGui/EditorSelection.h"
#include "Object/Class.h"
#include "Object/Reflection/ReflectionRegistry.h"
#include "World/Level.h"
#include "World/Node.h"
#include "World/World.h"

#include <imgui.h>

UNode& FHierarchyPanel::CreateNode(UWorld& World, const FString& BaseName)
{
	// 모든 Level에서 이름이 겹치지 않을 때까지 숫자 접미사를 증가시킨다.
	uint32 NameIndex = 0;
	while (true)
	{
		const FString Candidate = NameIndex == 0 ? BaseName : BaseName + " " + std::to_string(NameIndex);
		bool bNameExists = false;
		for (const TObjectPtr<ULevel>& LevelPointer : World.GetLevels())
		{
			const ULevel* Level = LevelPointer.Get();
			if (!Level)
			{
				continue;
			}
			for (const TObjectPtr<UNode>& NodePointer : Level->GetNodes())
			{
				if (NodePointer && NodePointer->GetName() == FName(Candidate))
				{
					bNameExists = true;
					break;
				}
			}
			if (bNameExists)
			{
				break;
			}
		}
		if (!bNameExists)
		{
			return World.GetPersistentLevel().CreateNode(FName(Candidate));
		}
		++NameIndex;
	}
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
	Selection.SelectedNode = &Node;
	return true;
}

void FHierarchyPanel::DrawLevel(UWorld& World, ULevel& Level, SIZE_T LevelIndex, FEditorSelection& Selection)
{
	ImGui::PushID(&Level);
	const FString LevelLabel = &Level == &World.GetPersistentLevel() ? "Persistent Level" : "Level " + std::to_string(LevelIndex);
	if (ImGui::TreeNodeEx(LevelLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
	{
		for (const TObjectPtr<UNode>& NodePointer : Level.GetNodes())
		{
			UNode* Node = NodePointer.Get();
			if (!Node)
			{
				continue;
			}

			const FString NodeName = Node->GetName().ToString();
			const bool bSelected = Selection.SelectedNode == Node;
			if (ImGui::Selectable(NodeName.c_str(), bSelected))
			{
				Selection.SelectedNode = Node;
			}
		}
		ImGui::TreePop();
	}
	ImGui::PopID();
}

// UCLASS 메타데이터로 노출된 Component를 선택하고 해당 Component를 가진 Node를 생성한다.
void FHierarchyPanel::DrawAddNode(UWorld& World, FEditorSelection& Selection)
{
	if (!ImGui::BeginPopupContextWindow("##HierarchyContextMenu", ImGuiPopupFlags_MouseButtonRight))
	{
		return;
	}

	if (ImGui::BeginMenu("Add Node"))
	{
		// Empty는 TransformComponent만 가진 기본 Node를 생성한다.
		if (ImGui::MenuItem("Empty"))
		{
			Selection.SelectedNode = &CreateNode(World, "Node");
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
	ImGui::EndPopup();
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

	DrawAddNode(World, Selection);
	ImGui::End();
}
