#include "ImGui/Panels/HierarchyPanel.h"

#include "ImGui/EditorSelection.h"
#include "World/Level.h"
#include "World/Node.h"
#include "World/World.h"

#include <imgui.h>

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
		ImGui::PushID(Level);
		const FString LevelLabel = Level == &World.GetPersistentLevel() ? "Persistent Level" : "Level " + std::to_string(LevelIndex);
		if (ImGui::TreeNodeEx(LevelLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			for (const TObjectPtr<UNode>& NodePointer : Level->GetNodes())
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
	ImGui::End();
}
