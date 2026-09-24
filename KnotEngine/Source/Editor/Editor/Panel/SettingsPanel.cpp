#include "Editor/Panel/SettingsPanel.h"

#include "Editor/Setting/EditorSettings.h"
#include "Runtime/EditorEngine.h"

#include <algorithm>
#include <imgui.h>

FSettingsPanel::FSettingsPanel()
	: Settings(GetEditor().GetEditorSettings())
{
}

void FSettingsPanel::Draw()
{
	static constexpr const char* PopupName = "Project Settings";
	if (bOpenRequested)
	{
		ImGui::OpenPopup(PopupName);
		bOpenRequested = false;
	}

	const ImGuiViewport* Viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowSize(ImVec2(720.0f, 430.0f), ImGuiCond_Appearing);
	ImGui::SetNextWindowPos(Viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	bool bKeepOpen = true;
	if (!ImGui::BeginPopupModal(PopupName, &bKeepOpen, ImGuiWindowFlags_NoCollapse))
	{
		return;
	}
	if (!bKeepOpen)
	{
		ImGui::CloseCurrentPopup();
	}

	ImGui::SetNextItemWidth(-70.0f);
	ImGui::InputTextWithHint("##SettingsSearch", "Search settings", SearchText.data(), SearchText.size());
	ImGui::SameLine();
	if (ImGui::Button("Reset"))
	{
		Settings.Reset();
		bLODStepsInvalid = false;
		bSaveFailed = !Settings.Save();
	}
	ImGui::Separator();

	const float CategoryWidth = 190.0f;
	ImGui::BeginChild("##SettingsCategories", ImVec2(CategoryWidth, 0.0f), ImGuiChildFlags_Borders);
	ImGui::Selectable("Graphics", true);
	ImGui::EndChild();
	ImGui::SameLine();
	ImGui::BeginChild("##SettingsDetails", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
	DrawGraphics();
	ImGui::EndChild();
	ImGui::EndPopup();
}

void FSettingsPanel::DrawGraphics()
{
	ImGui::TextUnformatted("Graphics");
	ImGui::Separator();
	ImGui::Spacing();
	ImGui::TextDisabled("LOD Screen Percentage");
	ImGui::TextWrapped("Set the projected screen percentage at which each lower LOD begins to render.");
	ImGui::Spacing();

	const TStaticArray<float, 4> PreviousLODSteps = Settings.LODSteps;
	bool bChanged = false;
	static constexpr ImGuiTableFlags TableFlags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings;
	if (ImGui::BeginTable("##LODSettings", 2, TableFlags))
	{
		ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthStretch, 0.55f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.45f);
		for (SIZE_T LODIndex = 0; LODIndex < Settings.LODSteps.size(); ++LODIndex)
		{
			float Percentage = Settings.LODSteps[LODIndex] * 100.0f;
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			const FString Label = "LOD " + std::to_string(LODIndex + 1);
			ImGui::TextUnformatted(Label.c_str());
			ImGui::TableSetColumnIndex(1);
			ImGui::PushID(static_cast<int>(LODIndex));
			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::DragFloat("##Percentage", &Percentage, 0.1f, 0.1f, 100.0f, "%.1f%%", ImGuiSliderFlags_AlwaysClamp))
			{
				Settings.LODSteps[LODIndex] = Percentage * 0.01f;
				bChanged = true;
			}
			ImGui::PopID();
		}
		ImGui::EndTable();
	}

	if (bChanged)
	{
		bLODStepsInvalid = !FEditorSettings::IsValidLODSteps(Settings.LODSteps);
		if (bLODStepsInvalid)
		{
			Settings.LODSteps = PreviousLODSteps;
		}
		else
		{
			bSaveFailed = !Settings.Save();
		}
	}
	if (bLODStepsInvalid)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.25f, 1.0f), "LOD screen percentages must be in strictly descending order.");
	}
	if (bSaveFailed)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.25f, 1.0f), "Failed to save Editor settings.");
	}
}
