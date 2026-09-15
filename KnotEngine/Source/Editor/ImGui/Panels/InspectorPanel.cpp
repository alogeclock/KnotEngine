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

FInspectorPanel::~FInspectorPanel()
{
	GUObjectManager.Destroy(CopiedComponent);
}

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

// Component 이름과 옵션 버튼을 포함한 접이식 헤더를 그린다.
bool FInspectorPanel::DrawComponentHeader(UComponent& Component, const FString& HeaderName, bool& bRemoveComponent)
{
	static constexpr float HeaderSpacing = 1.0f;
	static constexpr float PopupWidth = 180.0f;
	static constexpr float OptionButtonWidth = 18.0f;
	static constexpr float OptionDotRadius = 1.25f;
	static constexpr float OptionDotSpacing = 4.0f;
	check(BoldFont);

	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + HeaderSpacing);
	ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
	ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive));
	ImGui::PushFont(BoldFont);
	const bool bOpen = ImGui::CollapsingHeader(HeaderName.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
	ImGui::PopFont();
	ImGui::PopStyleColor(3);

	const ImVec2 HeaderMin = ImGui::GetItemRectMin();
	const ImVec2 HeaderMax = ImGui::GetItemRectMax();
	const ImVec2 CursorAfterHeader = ImGui::GetCursorScreenPos();
	ImGui::SetCursorScreenPos(ImVec2(HeaderMax.x - OptionButtonWidth, HeaderMin.y));
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
	if (ImGui::Button("##ComponentOptions", ImVec2(OptionButtonWidth, HeaderMax.y - HeaderMin.y)))
	{
		ImGui::OpenPopup("##ComponentContextMenu");
	}
	ImGui::PopStyleColor(3);
	const ImVec2 ButtonMin = ImGui::GetItemRectMin();
	const ImVec2 ButtonMax = ImGui::GetItemRectMax();
	const ImVec2 IconCenter((ButtonMin.x + ButtonMax.x) * 0.5f, (ButtonMin.y + ButtonMax.y) * 0.5f);
	const ImU32 IconColor = ImGui::GetColorU32(ImGuiCol_Text);
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->AddCircleFilled(ImVec2(IconCenter.x, IconCenter.y - OptionDotSpacing), OptionDotRadius, IconColor);
	DrawList->AddCircleFilled(IconCenter, OptionDotRadius, IconColor);
	DrawList->AddCircleFilled(ImVec2(IconCenter.x, IconCenter.y + OptionDotSpacing), OptionDotRadius, IconColor);
	ImGui::SetCursorScreenPos(CursorAfterHeader);

	const ImGuiViewport* Viewport = ImGui::GetWindowViewport();
	const float PopupHeight = ImGui::GetFrameHeight() * 3.0f + ImGui::GetStyle().WindowPadding.y * 2.0f + ImGui::GetStyle().ItemSpacing.y * 2.0f;
	const float SpaceLeft = ButtonMin.x - Viewport->WorkPos.x;
	const float SpaceRight = Viewport->WorkPos.x + Viewport->WorkSize.x - ButtonMax.x;
	const float SpaceAbove = ButtonMin.y - Viewport->WorkPos.y;
	const float SpaceBelow = Viewport->WorkPos.y + Viewport->WorkSize.y - ButtonMax.y;
	const bool bOpenToRight = SpaceRight >= PopupWidth || SpaceRight >= SpaceLeft;
	const bool bOpenBelow = SpaceBelow >= PopupHeight || SpaceBelow >= SpaceAbove;
	const ImVec2 PopupPosition(bOpenToRight ? ButtonMin.x : ButtonMax.x, bOpenBelow ? ButtonMax.y : ButtonMin.y);
	const ImVec2 PopupPivot(bOpenToRight ? 0.0f : 1.0f, bOpenBelow ? 0.0f : 1.0f);
	ImGui::SetNextWindowPos(PopupPosition, ImGuiCond_Appearing, PopupPivot);
	ImGui::SetNextWindowSize(ImVec2(PopupWidth, 0.0f), ImGuiCond_Appearing);
	if (ImGui::BeginPopup("##ComponentContextMenu"))
	{
		const bool bCanRemove = !Component.IsA(UTransformComponent::StaticClass());
		if (ImGui::MenuItem("Remove Component", nullptr, false, bCanRemove))
		{
			bRemoveComponent = true;
		}
		if (ImGui::MenuItem("Copy Component"))
		{
			CopyComponent(Component);
		}
		const bool bCanPaste = CopiedComponent && CopiedComponent->GetClass() == Component.GetClass();
		if (ImGui::MenuItem("Paste Component", nullptr, false, bCanPaste))
		{
			PasteComponent(Component);
		}
		ImGui::EndPopup();
	}
	return bOpen;
}

// Component의 편집 가능한 프로퍼티를 미등록 복사본에 저장한다.
void FInspectorPanel::CopyComponent(const UComponent& Component)
{
	UObject* CopiedObject = Component.GetClass()->CreateObject();
	panic(CopiedObject && CopiedObject->IsA(UComponent::StaticClass()));
	UComponent* NewCopiedComponent = static_cast<UComponent*>(CopiedObject);

	TArray<const FProperty*> Properties;
	Component.GetClass()->GetEditorProperties(Properties);
	for (const FProperty* Property : Properties)
	{
		check(Property);
		Property->CopyValue(Property->ContainerPtrToValuePtr(NewCopiedComponent), Property->ContainerPtrToValuePtr(&Component));
	}

	GUObjectManager.Destroy(CopiedComponent);
	CopiedComponent = NewCopiedComponent;
}

// 같은 클래스의 복사본에서 편집 가능한 프로퍼티를 붙여넣고 변경을 통지한다.
void FInspectorPanel::PasteComponent(UComponent& Component) const
{
	check(CopiedComponent && CopiedComponent->GetClass() == Component.GetClass());
	TArray<const FProperty*> Properties;
	Component.GetClass()->GetEditorProperties(Properties);
	for (const FProperty* Property : Properties)
	{
		check(Property);
		Property->CopyValue(Property->ContainerPtrToValuePtr(&Component), Property->ContainerPtrToValuePtr(CopiedComponent));
		Component.PostEditProperty(*Property);
	}
}

// Editor에서 선택된 객체의 프로퍼티를 그리는 패널을 구현한다.
void FInspectorPanel::Draw(const FEditorSelection& Selection)
{
	check(BoldFont);
	const ImVec2 InspectorPadding(0.0f, GetContentPadding());
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, InspectorPadding);
	const bool bVisible = ImGui::Begin("Inspector");
	ImGui::PopStyleVar();
	if (!bVisible)
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
	DrawObject(Node);
	UComponent* ComponentToRemove = nullptr;
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
		bool bRemoveComponent = false;
		if (DrawComponentHeader(*Component, HeaderName, bRemoveComponent))
		{
			DrawObject(*Component);
		}
		if (bRemoveComponent)
		{
			ComponentToRemove = Component;
		}
		ImGui::PopID();
	}
	if (ComponentToRemove)
	{
		Node.RemoveComponent(*ComponentToRemove);
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

// Inspector 콘텐츠의 위, 아래, 왼쪽과 오른쪽에 동일하게 적용할 여백을 반환한다.
float FInspectorPanel::GetContentPadding()
{
	const float CurrentLeftPadding = ImGui::GetTreeNodeToLabelSpacing() * 0.7f;
	const float CurrentRightPadding = ImGui::GetStyle().WindowPadding.x;
	return (CurrentLeftPadding + CurrentRightPadding) * 0.5f;
}

// Category 영역의 위쪽 여백과 이름을 그린다.
void FInspectorPanel::BeginCategory(const FString& CategoryName) const
{
	check(BoldFont);
	const float ContentPadding = GetContentPadding();
	const ImVec2 ItemSpacing = ImGui::GetStyle().ItemSpacing;
	ImGui::BeginGroup();
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ItemSpacing.x, 0.0f));
	ImGui::Dummy(ImVec2(0.0f, ItemSpacing.y));
	ImGui::PopStyleVar();
	ImGui::Indent(ContentPadding);
	ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + std::max(1.0f, ImGui::GetContentRegionAvail().x - ContentPadding));
	ImGui::PushFont(BoldFont);
	ImGui::TextUnformatted(CategoryName.c_str());
	ImGui::PopFont();
	ImGui::PopTextWrapPos();
	ImGui::Unindent(ContentPadding);
}

// Category 영역의 아래쪽 여백을 적용하고 닫는다.
void FInspectorPanel::EndCategory()
{
	const ImVec2 ItemSpacing = ImGui::GetStyle().ItemSpacing;
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ItemSpacing.x, 0.0f));
	ImGui::Dummy(ImVec2(0.0f, ItemSpacing.y));
	ImGui::PopStyleVar();
	ImGui::EndGroup();
}

// Editor에서 선택된 객체의 프로퍼티를 리플렉션 기반으로 그린다.
void FInspectorPanel::DrawObject(UObject& Object)
{
	TArray<const FProperty*> Properties;
	Object.GetClass()->GetEditorProperties(Properties);
	FString CurrentCategory;
	bool bCategoryOpen = false;
	for (const FProperty* Property : Properties)
	{
		if (Property)
		{
			const FString& Category = Property->GetMetadata().GetCategory();
			if (Category != CurrentCategory)
			{
				if (bCategoryOpen)
				{
					EndCategory();
				}
				CurrentCategory = Category;
				bCategoryOpen = !CurrentCategory.empty();
				if (bCategoryOpen)
				{
					BeginCategory(CurrentCategory);
				}
			}
			DrawProperty(Object, *Property, &Object);
		}
	}
	if (bCategoryOpen)
	{
		EndCategory();
	}
}

// 좌우 여백 안에서 프로퍼티 이름과 값을 25:75 비율의 한 행으로 시작한다.
bool FInspectorPanel::BeginPropertyRow(const char* Label)
{
	static constexpr float PropertySpacing = 2.0f;
	static constexpr ImGuiTableFlags TableFlags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings;
	const ImGuiStyle& Style = ImGui::GetStyle();
	const float ContentPadding = GetContentPadding();
	ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(Style.CellPadding.x, 0.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(Style.ItemSpacing.x, PropertySpacing));
	ImGui::Indent(ContentPadding);
	const float TableWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x - ContentPadding);
	if (!ImGui::BeginTable("##PropertyRow", 2, TableFlags, ImVec2(TableWidth, 0.0f)))
	{
		ImGui::Unindent(ContentPadding);
		ImGui::PopStyleVar(2);
		return false;
	}
	ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthStretch, 0.25f);
	ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.75f);
	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(Label);
	ImGui::TableNextColumn();
	ImGui::AlignTextToFramePadding();
	ImGui::SetNextItemWidth(-FLT_MIN);
	return true;
}

// 현재 프로퍼티 행의 테이블을 닫는다.
void FInspectorPanel::EndPropertyRow()
{
	ImGui::EndTable();
	ImGui::Unindent(GetContentPadding());
	ImGui::PopStyleVar(2);
}

bool FInspectorPanel::DrawVector(const char* Label, FVector& Vector)
{
	ImGui::PushID(Label);
	bool bChanged = false;
	if (BeginPropertyRow(Label))
	{
		if (ImGui::BeginTable("##VectorValues", 3, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
		{
			ImGui::TableNextRow();
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
		EndPropertyRow();
	}
	ImGui::PopID();
	return bChanged;
}

// Rotator의 Pitch, Yaw, Roll 값을 한 행에 나란히 그린다.
bool FInspectorPanel::DrawRotator(const char* Label, FRotator& Rotator)
{
	ImGui::PushID(Label);
	bool bChanged = false;
	if (BeginPropertyRow(Label))
	{
		if (ImGui::BeginTable("##RotatorValues", 3, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-FLT_MIN);
			bChanged |= ImGui::DragFloat("##Pitch", &Rotator.Pitch, 0.1f, 0.0f, 0.0f, "%.1f°");
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-FLT_MIN);
			bChanged |= ImGui::DragFloat("##Yaw", &Rotator.Yaw, 0.1f, 0.0f, 0.0f, "%.1f°");
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-FLT_MIN);
			bChanged |= ImGui::DragFloat("##Roll", &Rotator.Roll, 0.1f, 0.0f, 0.0f, "%.1f°");
			ImGui::EndTable();
		}
		EndPropertyRow();
	}
	ImGui::PopID();
	return bChanged;
}

bool FInspectorPanel::DrawQuat(const char* Label, FQuat& Quat)
{
	ImGui::PushID(Label);
	ImGuiStorage* Storage = ImGui::GetStateStorage();
	const ImGuiID InitializedId = ImGui::GetID("##QuatInitialized");
	const ImGuiID EditingId = ImGui::GetID("##QuatEditing");
	const ImGuiID EulerXId = ImGui::GetID("##QuatEulerX");
	const ImGuiID EulerYId = ImGui::GetID("##QuatEulerY");
	const ImGuiID EulerZId = ImGui::GetID("##QuatEulerZ");
	const ImGuiID QuatXId = ImGui::GetID("##QuatX");
	const ImGuiID QuatYId = ImGui::GetID("##QuatY");
	const ImGuiID QuatZId = ImGui::GetID("##QuatZ");
	const ImGuiID QuatWId = ImGui::GetID("##QuatW");

	const bool bInitialized = Storage->GetBool(InitializedId);
	const bool bWasEditing = Storage->GetBool(EditingId);
	const FQuat CachedQuat(
		Storage->GetFloat(QuatXId),
		Storage->GetFloat(QuatYId),
		Storage->GetFloat(QuatZId),
		Storage->GetFloat(QuatWId, 1.0f));
	FVector Euler;
	if (bInitialized && (bWasEditing || Quat.Equals(CachedQuat)))
	{
		Euler = FVector(Storage->GetFloat(EulerXId), Storage->GetFloat(EulerYId), Storage->GetFloat(EulerZId));
	}
	else
	{
		const FRotator Rotator = Quat.Rotator();
		Euler = FVector(Rotator.Roll, Rotator.Pitch, Rotator.Yaw);
	}

	bool bChanged = false;
	bool bEditing = false;
	if (BeginPropertyRow(Label))
	{
		if (ImGui::BeginTable("##QuatValues", 3, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-FLT_MIN);
			float EditedX = Euler.X;
			if (ImGui::DragFloat("X##Value", &EditedX, 0.1f))
			{
				const FQuat DeltaRotation(FVector::ForwardVector, KMath::ToRadian(EditedX - Euler.X));
				Quat = (Quat * DeltaRotation).GetNormalized();
				Euler.X = EditedX;
				bChanged = true;
			}
			bEditing |= ImGui::IsItemActive();
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-FLT_MIN);
			float EditedY = Euler.Y;
			if (ImGui::DragFloat("Y##Value", &EditedY, 0.1f))
			{
				const FQuat YawRotation(FVector::UpVector, KMath::ToRadian(Euler.Z));
				const FVector PitchAxis = YawRotation.RotateVector(FVector::RightVector);
				const FQuat DeltaRotation(PitchAxis, KMath::ToRadian(EditedY - Euler.Y));
				Quat = (DeltaRotation * Quat).GetNormalized();
				Euler.Y = EditedY;
				bChanged = true;
			}
			bEditing |= ImGui::IsItemActive();
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-FLT_MIN);
			float EditedZ = Euler.Z;
			if (ImGui::DragFloat("Z##Value", &EditedZ, 0.1f))
			{
				const FQuat DeltaRotation(FVector::UpVector, KMath::ToRadian(EditedZ - Euler.Z));
				Quat = (DeltaRotation * Quat).GetNormalized();
				Euler.Z = EditedZ;
				bChanged = true;
			}
			bEditing |= ImGui::IsItemActive();
			ImGui::EndTable();
		}
		EndPropertyRow();
	}
	if (bChanged && Euler.IsNearlyZero())
	{
		Quat = FQuat::Identity;
	}

	Storage->SetBool(InitializedId, true);
	Storage->SetBool(EditingId, bEditing);
	Storage->SetFloat(EulerXId, Euler.X);
	Storage->SetFloat(EulerYId, Euler.Y);
	Storage->SetFloat(EulerZId, Euler.Z);
	Storage->SetFloat(QuatXId, Quat.X);
	Storage->SetFloat(QuatYId, Quat.Y);
	Storage->SetFloat(QuatZId, Quat.Z);
	Storage->SetFloat(QuatWId, Quat.W);
	ImGui::PopID();
	return bChanged;
}

// Transform의 Translation, Rotation과 Scale을 각각 한 행에 그린다.
bool FInspectorPanel::DrawTransform(FTransform& Transform)
{
	bool bChanged = false;
	bChanged |= DrawVector("Translation", Transform.Translation);
	bChanged |= DrawQuat("Rotation", Transform.Rotation);
	bChanged |= DrawVector("Scale", Transform.Scale);
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
		if (BeginPropertyRow(Label.c_str()))
		{
			bChanged = ImGui::DragInt("##Value", &EditedValue);
			EndPropertyRow();
		}
		if (bChanged)
		{
			Property.CopyValue(Value, &EditedValue);
		}
		break;
	}
	case EPropertyKind::Bool:
	{
		bool bEditedValue = *static_cast<bool*>(Value);
		if (BeginPropertyRow(Label.c_str()))
		{
			bChanged = ImGui::Checkbox("##Value", &bEditedValue);
			EndPropertyRow();
		}
		if (bChanged)
		{
			Property.CopyValue(Value, &bEditedValue);
		}
		break;
	}
	case EPropertyKind::Float:
	{
		float EditedValue = *static_cast<float*>(Value);
		if (BeginPropertyRow(Label.c_str()))
		{
			bChanged = ImGui::DragFloat("##Value", &EditedValue, 0.1f);
			EndPropertyRow();
		}
		if (bChanged)
		{
			Property.CopyValue(Value, &EditedValue);
		}
		break;
	}
	case EPropertyKind::Double:
	{
		double EditedValue = *static_cast<double*>(Value);
		if (BeginPropertyRow(Label.c_str()))
		{
			bChanged = ImGui::DragScalar("##Value", ImGuiDataType_Double, &EditedValue, 0.1f);
			EndPropertyRow();
		}
		if (bChanged)
		{
			Property.CopyValue(Value, &EditedValue);
		}
		break;
	}
	case EPropertyKind::String:
	{
		FString EditedValue = *static_cast<FString*>(Value);
		if (BeginPropertyRow(Label.c_str()))
		{
			bChanged = ImGui::InputText("##Value", &EditedValue);
			EndPropertyRow();
		}
		if (bChanged)
		{
			Property.CopyValue(Value, &EditedValue);
		}
		break;
	}
	case EPropertyKind::Name:
	{
		FString EditedText = static_cast<FName*>(Value)->ToString();
		if (BeginPropertyRow(Label.c_str()))
		{
			bChanged = ImGui::InputText("##Value", &EditedText);
			EndPropertyRow();
		}
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
		if (BeginPropertyRow(Label.c_str()))
		{
			if (ImGui::BeginCombo("##Value", Preview.c_str()))
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
			EndPropertyRow();
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
		else if (Struct == FRotator::StaticStruct())
		{
			FRotator EditedValue = *static_cast<FRotator*>(Value);
			bChanged = DrawRotator(Label.c_str(), EditedValue);
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
			bChanged = DrawTransform(EditedValue);
			bTooltipHandled = true;
			if (bChanged)
			{
				Property.CopyValue(Value, &EditedValue);
			}
		}
		else
		{
			check(BoldFont);
			ImGui::PushFont(BoldFont);
			const bool bOpen = ImGui::CollapsingHeader(Label.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
			ImGui::PopFont();
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
					if (Member)
					{
						bChanged |= DrawProperty(Object, *Member, Value, false);
					}
				}
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

			if (BeginPropertyRow(Label.c_str()))
			{
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
				EndPropertyRow();
			}
		}
		else
		{
			if (BeginPropertyRow(Label.c_str()))
			{
				ImGui::TextUnformatted(ReferencedObject ? ReferencedObject->GetClass()->GetName().c_str() : "None");
				EndPropertyRow();
			}
		}
		break;
	}
	case EPropertyKind::SoftObject:
	{
		const FSoftObjectProperty& SoftProperty = static_cast<const FSoftObjectProperty&>(Property);
		if (BeginPropertyRow(Label.c_str()))
		{
			ImGui::TextUnformatted(SoftProperty.GetSoftObjectPtrOps()->GetPath(Value).c_str());
			EndPropertyRow();
		}
		break;
	}
	case EPropertyKind::Array:
	{
		if (BeginPropertyRow(Label.c_str()))
		{
			ImGui::TextUnformatted("Array");
			EndPropertyRow();
		}
		break;
	}
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
