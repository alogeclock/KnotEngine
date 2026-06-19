#include "EditorUI.h"

void FEditorUI::Draw(float DeltaTime)
{
	ImGui::Begin("Knot Engine Property Window");
	ImGui::Text("Hello, Knot Engine!");
	
	ImGui::Separator();

	static float clear_color[4] = { 0.025f, 0.025f, 0.025f, 1.0f };

	ImGui::ColorEdit4("Background Color", clear_color);

	ImGui::End();
}
