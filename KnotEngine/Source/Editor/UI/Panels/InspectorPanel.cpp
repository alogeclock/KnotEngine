#include "UI/Panels/InspectorPanel.h"

#include "Component/Component.h"
#include "Core/Geometry/Transform.h"
#include "Core/Math/Rotator.h"
#include "Object/Class.h"
#include "Object/Property.h"
#include "Object/Property/EnumProperty.h"
#include "Object/Property/ObjectProperty.h"
#include "Object/Property/SoftObjectProperty.h"
#include "Object/Property/StructProperty.h"
#include "UI/EditorSelection.h"
#include "World/Node.h"

#include <imgui.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <cfloat>
#include <cstring>

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
		FString HeaderName = ClassName;
		if (ClassName.size() > 10 && ClassName.starts_with('U') && ClassName.ends_with("Component"))
		{
			HeaderName = ClassName.substr(1, ClassName.size() - 10);
		}
		ImGui::PushID(Component);
		if (ImGui::CollapsingHeader(HeaderName.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			DrawObject(*Component);
		}
		ImGui::PopID();
	}
	ImGui::End();
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

bool FInspectorPanel::DrawTransform(const char* Label, FTransform& Transform)
{
	if (!ImGui::TreeNodeEx(Label, ImGuiTreeNodeFlags_DefaultOpen))
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
			bChanged = DrawTransform(Label.c_str(), EditedValue);
			if (bChanged)
			{
				Property.CopyValue(Value, &EditedValue);
			}
		}
		else if (ImGui::TreeNodeEx(Label.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			TArray<const FProperty*> Members;
			Struct->GetEditorProperties(Members);
			for (const FProperty* Member : Members)
			{
				bChanged |= Member && DrawProperty(Object, *Member, Value, false);
			}
			ImGui::TreePop();
		}
		break;
	}
	case EPropertyKind::Object:
	{
		const FObjectProperty& ObjectProperty = static_cast<const FObjectProperty&>(Property);
		UObject* ReferencedObject = ObjectProperty.GetObjectPtrOps()->GetObject(Value);
		ImGui::LabelText(Label.c_str(), "%s", ReferencedObject ? ReferencedObject->GetClass()->GetName().c_str() : "None");
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
	if (!Metadata.GetTooltip().empty() && ImGui::IsItemHovered())
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
