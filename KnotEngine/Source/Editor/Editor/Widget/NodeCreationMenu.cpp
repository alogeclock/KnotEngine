#include "Editor/Widget/NodeCreationMenu.h"

#include "Component/Component.h"
#include "Object/Reflection/Class.h"
#include "Object/Reflection/ReflectionRegistry.h"
#include "World/Level.h"
#include "World/Node.h"
#include "World/World.h"

#include <imgui.h>

// 고유한 이름으로 Persistent Level에 기본 Node를 생성한다.
UNode& FNodeCreationMenu::CreateNode(UWorld& World, const FString& BaseName)
{
	return World.GetPersistentLevel().CreateNode(World.GetNodeName(BaseName));
}

// 선택한 Component Class를 가진 기본 Node를 생성한다.
UNode* FNodeCreationMenu::DrawComponentNode(UWorld& World, const UClass& ComponentClass)
{
	const FString& DisplayName = ComponentClass.GetMetadata().GetDisplayName();
	if (!ImGui::MenuItem(DisplayName.c_str()))
	{
		return nullptr;
	}

	UNode& Node = CreateNode(World, DisplayName);
	Node.AddComponent(ComponentClass);
	return &Node;
}

// 공유 Node 항목을 Add Node 하위 메뉴로 표시한다.
UNode* FNodeCreationMenu::Draw(UWorld& World)
{
	if (!ImGui::BeginMenu("Add Node"))
	{
		return nullptr;
	}
	UNode* CreatedNode = DrawItems(World);
	ImGui::EndMenu();
	return CreatedNode;
}

// Empty와 EditorSpawnable Component를 Category별 항목으로 표시한다.
UNode* FNodeCreationMenu::DrawItems(UWorld& World)
{
	UNode* CreatedNode = nullptr;
	if (ImGui::MenuItem("Empty"))
	{
		return &CreateNode(World, "Node");
	}

	check(GReflectionRegistry);
	const TArray<const UClass*>& ComponentClasses = GReflectionRegistry->GetEditorSpawnableClasses();
	for (SIZE_T FirstClass = 0; FirstClass < ComponentClasses.size() && !CreatedNode;)
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
				CreatedNode = DrawComponentNode(World, *ComponentClasses[ClassIndex]);
				if (CreatedNode)
				{
					break;
				}
			}
			ImGui::EndMenu();
		}
		FirstClass = LastClass;
	}

	return CreatedNode;
}
