#include "ImGui/Panels/InspectorPanel.h"

#include "Asset/AssetManager.h"
#include "Asset/GeometryMesh.h"
#include "Component/Component.h"
#include "Core/Geometry/Transform.h"
#include "Core/Math/Rotator.h"
#include "Object/Class.h"
#include "Object/Property.h"
#include "Object/Property/EnumProperty.h"
#include "Object/Property/ObjectProperty.h"
#include "Object/Property/SoftObjectProperty.h"
#include "Object/Property/StructProperty.h"
#include "Object/Reflection/ReflectionRegistry.h"
#include "ImGui/EditorSelection.h"
#include "World/Node.h"

#include <imgui.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cstring>

bool FInspectorPanel::DrawComponent(UNode& Node, const UClass& Class)
{
	const FString& DisplayName = Class.GetMetadata().GetDisplayName();
	if (!ImGui::Selectable(DisplayName.c_str()))
	{
		return false;
	}
	Node.AddComponent(Class);
	ImGui::CloseCurrentPopup();
	return true;
}

// Editor에서 선택된 객체의 프로퍼티를 그리는 패널을 구현한다.
void FInspectorPanel::Draw(const FEditorSelection& Selection)
{
	if (!ImGui::Begin("Inspector"))
	{
		ImGui::End();
		return;
	}
	if (!Selection.SelectedNode)
	{
		ImGui::TextDisabled("Select a node in Hierarchy.");
		ImGui::End();
		return;
	}

	UNode& Node = *Selection.SelectedNode;
	ImGui::TextUnformatted(Node.GetName().ToString().c_str());
	ImGui::Separator();
	DrawObject(Node);
	for (const TObjectPtr<UComponent>& ComponentPointer : Node.GetComponents())
	{
		UComponent* Component = ComponentPointer.Get();
		if (!Component)
		{
			continue;
		}
		const FString ClassName = Component->GetClass()->GetName();
		FString HeaderName = Component->GetClass()->GetMetadata().GetDisplayName();
		if (HeaderName.empty())
		{
			HeaderName = ClassName;
			if (ClassName.size() > 10 && ClassName.starts_with('U') && ClassName.ends_with("Component"))
			{
				HeaderName = ClassName.substr(1, ClassName.size() - 10);
			}
		}
		ImGui::PushID(Component);
		if (ImGui::CollapsingHeader(HeaderName.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			DrawObject(*Component);
		}
		ImGui::PopID();
	}
	DrawAddComponent(Node);
	ImGui::End();
}

// UCLASS 메타데이터로 노출된 Component를 검색하고 선택한 클래스를 현재 Node에 생성한다.
void FInspectorPanel::DrawAddComponent(UNode& Node)
{
	static constexpr float ControlWidth = 260.0f;
	static constexpr float PopupHeight = 360.0f;
	static constexpr float SectionSpacing = 10.0f;

	// Inspector 너비와 버튼 크기를 맞추고 가운데에 배치한다.
	ImGui::Dummy(ImVec2(0.0f, SectionSpacing));
	const float AvailableWidth = ImGui::GetContentRegionAvail().x;
	const float ActualButtonWidth = std::min(ControlWidth, AvailableWidth);
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (AvailableWidth - ActualButtonWidth) * 0.5f));
	if (ImGui::Button("Add Component", ImVec2(ActualButtonWidth, 0.0f)))
	{
		ImGui::OpenPopup("##AddComponentPopup");
	}

	// 화면 경계 안에서 공간이 더 넓은 방향으로 팝업을 연다.
	const ImVec2 ButtonMin = ImGui::GetItemRectMin();
	const ImVec2 ButtonMax = ImGui::GetItemRectMax();
	const ImGuiViewport* Viewport = ImGui::GetWindowViewport();
	const ImVec2 PopupSize(ActualButtonWidth, std::min(PopupHeight, Viewport->WorkSize.y));
	const float SpaceAbove = ButtonMin.y - Viewport->WorkPos.y;
	const float SpaceBelow = Viewport->WorkPos.y + Viewport->WorkSize.y - ButtonMax.y;
	const bool bOpenBelow = SpaceBelow >= PopupSize.y || SpaceBelow >= SpaceAbove;
	const ImVec2 PopupPosition(ButtonMin.x, bOpenBelow ? ButtonMax.y : ButtonMin.y);
	const ImVec2 PopupPivot(0.0f, bOpenBelow ? 0.0f : 1.0f);
	ImGui::SetNextWindowPos(PopupPosition, ImGuiCond_Appearing, PopupPivot);
	ImGui::SetNextWindowSize(PopupSize, ImGuiCond_Appearing);
	if (!ImGui::BeginPopup("##AddComponentPopup"))
	{
		return;
	}

	// 팝업을 처음 열었을 때 바로 Component 이름을 입력할 수 있도록 한다.
	if (ImGui::IsWindowAppearing())
	{
		ImGui::SetKeyboardFocusHere();
	}
	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::InputTextWithHint("##ComponentFilter", "Search Component", ComponentFilter.data(), ComponentFilter.size());
	ImGui::Spacing();

	// Registry가 등록 시점에 정렬한 EditorSpawnable Class 목록을 참조한다.
	check(GReflectionRegistry);
	const TArray<const UClass*>& ComponentClasses = GReflectionRegistry->GetEditorSpawnableClasses();

	const FString FilterText(ComponentFilter.data());
	// Component 이름과 Category를 대소문자 구분 없이 검색한다.
	const auto ContainsFilter = [&FilterText](const FString& Text)
	{
		return std::search(Text.begin(), Text.end(), FilterText.begin(), FilterText.end(), [](char Left, char Right)
		{
			return std::tolower(static_cast<unsigned char>(Left)) == std::tolower(static_cast<unsigned char>(Right));
		}) != Text.end();
	};

	bool bHasMatch = false;
	bool bComponentAdded = false;
	if (!FilterText.empty())
	{
		// 검색 중에는 Category 메뉴를 생략하고 일치한 Component를 바로 표시한다.
		for (const UClass* Class : ComponentClasses)
		{
			if (!Class->IsChildOf(UComponent::StaticClass()))
			{
				continue;
			}

			const FString& DisplayName = Class->GetMetadata().GetDisplayName();
			const FString& Category = Class->GetMetadata().GetCategory();
			if (ContainsFilter(DisplayName) || ContainsFilter(Category))
			{
				bHasMatch = true;
				if (DrawComponent(Node, *Class))
				{
					bComponentAdded = true;
					break;
				}
			}
		}
	}
	else
	{
		// 검색어가 없으면 정렬된 Component를 같은 Category 단위로 묶는다.
		for (SIZE_T FirstClass = 0; FirstClass < ComponentClasses.size();)
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

			bHasMatch = true;
			const FString MenuName = Category.empty() ? "Other" : Category;
			if (ImGui::BeginMenu(MenuName.c_str()))
			{
				for (SIZE_T ClassIndex = FirstClass; ClassIndex < LastClass; ++ClassIndex)
				{
					if (!ComponentClasses[ClassIndex]->IsChildOf(UComponent::StaticClass()))
					{
						continue;
					}
					if (DrawComponent(Node, *ComponentClasses[ClassIndex]))
					{
						bComponentAdded = true;
						break;
					}
				}
				ImGui::EndMenu();
			}
			if (bComponentAdded)
			{
				break;
			}
			FirstClass = LastClass;
		}
	}

	if (!bHasMatch)
	{
		ImGui::TextDisabled("No components found.");
	}
	if (bComponentAdded)
	{
		ComponentFilter.fill('\0'); // 컴포넌트 필터를 초기화한다.
	}
	ImGui::EndPopup();
}

// Editor에서 선택된 객체의 프로퍼티를 리플렉션 기반으로 그린다.
void FInspectorPanel::DrawObject(UObject& Object)
{
	TArray<const FProperty*> Properties;
	Object.GetClass()->GetEditorProperties(Properties);
	FString CurrentCategory;
	for (const FProperty* Property : Properties)
	{
		if (Property)
		{
			const FString& Category = Property->GetMetadata().GetCategory();
			if (!Category.empty() && Category != CurrentCategory)
			{
				CurrentCategory = Category;
				ImGui::SeparatorText(CurrentCategory.c_str());
			}
			DrawProperty(Object, *Property, &Object);
		}
	}
}

bool FInspectorPanel::DrawVector(const char* Label, FVector& Vector)
{
	ImGui::PushID(Label);
	const float LabelColumnWidth = std::max(ImGui::CalcTextSize(Label).x, ImGui::CalcTextSize("Translation").x) + ImGui::GetStyle().CellPadding.x * 2.0f;
	const bool bVisible = ImGui::BeginTable("##Vector", 4, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings);
	bool bChanged = false;
	if (bVisible)
	{
		ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, LabelColumnWidth);
		ImGui::TableSetupColumn("X", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Y", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Z", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(Label);
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-FLT_MIN);
		bChanged |= ImGui::DragFloat("X##Value", &Vector.X, 0.1f);
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-FLT_MIN);
		bChanged |= ImGui::DragFloat("Y##Value", &Vector.Y, 0.1f);
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-FLT_MIN);
		bChanged |= ImGui::DragFloat("Z##Value", &Vector.Z, 0.1f);
		ImGui::EndTable();
	}
	ImGui::PopID();
	return bChanged;
}

bool FInspectorPanel::DrawQuat(const char* Label, FQuat& Quat)
{
	FRotator Rotator = Quat.Rotator();
	ImGui::PushID(Label);
	const float LabelColumnWidth = std::max(ImGui::CalcTextSize(Label).x, ImGui::CalcTextSize("Translation").x) + ImGui::GetStyle().CellPadding.x * 2.0f;
	const bool bVisible = ImGui::BeginTable("##Quat", 4, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings);
	bool bChanged = false;
	if (bVisible)
	{
		ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, LabelColumnWidth);
		ImGui::TableSetupColumn("Pitch", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Yaw", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Roll", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(Label);
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-FLT_MIN);
		bChanged |= ImGui::DragFloat("Pitch##Value", &Rotator.Pitch, 0.1f);
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-FLT_MIN);
		bChanged |= ImGui::DragFloat("Yaw##Value", &Rotator.Yaw, 0.1f);
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-FLT_MIN);
		bChanged |= ImGui::DragFloat("Roll##Value", &Rotator.Roll, 0.1f);
		ImGui::EndTable();
	}
	ImGui::PopID();
	if (bChanged)
	{
		Quat = Rotator.Quaternion().GetNormalized();
	}
	return bChanged;
}

bool FInspectorPanel::DrawTransform(const char* Label, const char* Tooltip, FTransform& Transform)
{
	const bool bOpen = ImGui::TreeNodeEx(Label, ImGuiTreeNodeFlags_DefaultOpen);
	if (Tooltip[0] != '\0' && ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("%s", Tooltip);
	}
	if (!bOpen)
	{
		return false;
	}
	bool bChanged = false;
	bChanged |= DrawVector("Translation", Transform.Translation);
	bChanged |= DrawQuat("Rotation", Transform.Rotation);
	bChanged |= DrawVector("Scale", Transform.Scale);
	ImGui::TreePop();
	return bChanged;
}

// Editor에서 객체의 속성을 프로퍼티를 어떻게 그릴지 결정한다.
// ImGui를 사용하여 다양한 속성 타입에 따라 적절한 UI 위젯을 생성하고, 값이 변경되면 해당 값을 업데이트한다.
bool FInspectorPanel::DrawProperty(UObject& Object, const FProperty& Property, void* Container, bool bNotifyObject)
{
	void* Value = Property.ContainerPtrToValuePtr(Container);
	const FReflectionMetadata& Metadata = Property.GetMetadata();
	const FString Label = Metadata.GetDisplayName().empty() ? Property.GetName() : Metadata.GetDisplayName();
	ImGui::PushID(&Property);
	bool bChanged = false;
	bool bTooltipHandled = false;
	switch (Property.GetKind())
	{
	case EPropertyKind::Int32:
	{
		int32 EditedValue = *static_cast<int32*>(Value);
		bChanged = ImGui::DragInt(Label.c_str(), &EditedValue);
		if (bChanged)
		{
			Property.CopyValue(Value, &EditedValue);
		}
		break;
	}
	case EPropertyKind::Bool:
	{
		bool bEditedValue = *static_cast<bool*>(Value);
		bChanged = ImGui::Checkbox(Label.c_str(), &bEditedValue);
		if (bChanged)
		{
			Property.CopyValue(Value, &bEditedValue);
		}
		break;
	}
	case EPropertyKind::Float:
	{
		float EditedValue = *static_cast<float*>(Value);
		bChanged = ImGui::DragFloat(Label.c_str(), &EditedValue, 0.1f);
		if (bChanged)
		{
			Property.CopyValue(Value, &EditedValue);
		}
		break;
	}
	case EPropertyKind::Double:
	{
		double EditedValue = *static_cast<double*>(Value);
		bChanged = ImGui::DragScalar(Label.c_str(), ImGuiDataType_Double, &EditedValue, 0.1f);
		if (bChanged)
		{
			Property.CopyValue(Value, &EditedValue);
		}
		break;
	}
	case EPropertyKind::String:
	{
		FString EditedValue = *static_cast<FString*>(Value);
		bChanged = ImGui::InputText(Label.c_str(), &EditedValue);
		if (bChanged)
		{
			Property.CopyValue(Value, &EditedValue);
		}
		break;
	}
	case EPropertyKind::Name:
	{
		FString EditedText = static_cast<FName*>(Value)->ToString();
		bChanged = ImGui::InputText(Label.c_str(), &EditedText);
		if (bChanged)
		{
			const FName EditedValue(EditedText);
			Property.CopyValue(Value, &EditedValue);
		}
		break;
	}
	case EPropertyKind::Enum:
	{
		const FEnumProperty& EnumProperty = static_cast<const FEnumProperty&>(Property);
		const UEnum& Enum = *EnumProperty.GetEnum();
		int64 CurrentValue = 0;
		std::memcpy(&CurrentValue, Value, Enum.GetSize());
		const FEnumValue* Current = nullptr;
		for (const FEnumValue& EnumValue : Enum.GetValues())
		{
			if (EnumValue.Value == CurrentValue)
			{
				Current = &EnumValue;
				break;
			}
		}
		const FString Preview = Current ? (Current->DisplayName.empty() ? Current->Name.ToString() : Current->DisplayName) : "Unknown";
		if (ImGui::BeginCombo(Label.c_str(), Preview.c_str()))
		{
			for (const FEnumValue& EnumValue : Enum.GetValues())
			{
				const FString EnumLabel = EnumValue.DisplayName.empty() ? EnumValue.Name.ToString() : EnumValue.DisplayName;
				if (ImGui::Selectable(EnumLabel.c_str(), EnumValue.Value == CurrentValue))
				{
					Property.CopyValue(Value, &EnumValue.Value);
					bChanged = true;
				}
			}
			ImGui::EndCombo();
		}
		break;
	}
	case EPropertyKind::Struct:
	{
		const FStructProperty& StructProperty = static_cast<const FStructProperty&>(Property);
		const UScriptStruct* Struct = StructProperty.GetStruct();
		if (Struct == FVector::StaticStruct())
		{
			FVector EditedValue = *static_cast<FVector*>(Value);
			bChanged = DrawVector(Label.c_str(), EditedValue);
			if (bChanged)
			{
				Property.CopyValue(Value, &EditedValue);
			}
		}
		else if (Struct == FQuat::StaticStruct())
		{
			FQuat EditedValue = *static_cast<FQuat*>(Value);
			bChanged = DrawQuat(Label.c_str(), EditedValue);
			if (bChanged)
			{
				Property.CopyValue(Value, &EditedValue);
			}
		}
		else if (Struct == FTransform::StaticStruct())
		{
			FTransform EditedValue = *static_cast<FTransform*>(Value);
			bChanged = DrawTransform(Label.c_str(), Metadata.GetTooltip().c_str(), EditedValue);
			bTooltipHandled = true;
			if (bChanged)
			{
				Property.CopyValue(Value, &EditedValue);
			}
		}
		else
		{
			const bool bOpen = ImGui::TreeNodeEx(Label.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
			if (!Metadata.GetTooltip().empty() && ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("%s", Metadata.GetTooltip().c_str());
			}
			bTooltipHandled = true;
			if (bOpen)
			{
				TArray<const FProperty*> Members;
				Struct->GetEditorProperties(Members);
				for (const FProperty* Member : Members)
				{
					bChanged |= Member && DrawProperty(Object, *Member, Value, false);
				}
				ImGui::TreePop();
			}
		}
		break;
	}
	case EPropertyKind::Object:
	{
		const FObjectProperty& ObjectProperty = static_cast<const FObjectProperty&>(Property);
		UObject* ReferencedObject = ObjectProperty.GetObjectPtrOps()->GetObject(Value);
		if (ObjectProperty.GetPropertyClass() == UGeometryMesh::StaticClass())
		{
			check(GAssetManager);
			const UGeometryMesh* CurrentMesh = static_cast<const UGeometryMesh*>(ReferencedObject);
			const char* Preview = CurrentMesh ? CurrentMesh->GetDisplayName() : "None";
			TArray<UGeometryMesh*> GeometryMeshes;
			GeometryMeshes.reserve(GAssetManager->GetGeometryMeshCache().size());
			for (const auto& Entry : GAssetManager->GetGeometryMeshCache())
			{
				if (UGeometryMesh* GeometryMesh = Entry.second.Get())
				{
					GeometryMeshes.push_back(GeometryMesh);
				}
			}
			std::sort(GeometryMeshes.begin(), GeometryMeshes.end(), [](const UGeometryMesh* Left, const UGeometryMesh* Right)
			{
				return std::strcmp(Left->GetDisplayName(), Right->GetDisplayName()) < 0;
			});

			const bool bVisible = ImGui::BeginTable("##GeometryMesh", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings);
			if (bVisible)
			{
				ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthStretch, 0.25f);
				ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.75f);
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(Label.c_str());
				ImGui::TableNextColumn();
				ImGui::SetNextItemWidth(-FLT_MIN);
				if (ImGui::BeginCombo("##Value", Preview))
				{
					for (UGeometryMesh* GeometryMesh : GeometryMeshes)
					{
						const bool bSelected = CurrentMesh == GeometryMesh;
						if (ImGui::Selectable(GeometryMesh->GetDisplayName(), bSelected))
						{
							ObjectProperty.GetObjectPtrOps()->SetObject(Value, GeometryMesh);
							bChanged = true;
						}
					}
					ImGui::EndCombo();
				}
				ImGui::EndTable();
			}
		}
		else
		{
			ImGui::LabelText(Label.c_str(), "%s", ReferencedObject ? ReferencedObject->GetClass()->GetName().c_str() : "None");
		}
		break;
	}
	case EPropertyKind::SoftObject:
	{
		const FSoftObjectProperty& SoftProperty = static_cast<const FSoftObjectProperty&>(Property);
		ImGui::LabelText(Label.c_str(), "%s", SoftProperty.GetSoftObjectPtrOps()->GetPath(Value).c_str());
		break;
	}
	case EPropertyKind::Array: ImGui::LabelText(Label.c_str(), "%s", "Array"); break;
	}
	if (!bTooltipHandled && !Metadata.GetTooltip().empty() && ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("%s", Metadata.GetTooltip().c_str());
	}
	if (bChanged && bNotifyObject)
	{
		Object.PostEditProperty(Property);
	}
	ImGui::PopID();
	return bChanged;
}
