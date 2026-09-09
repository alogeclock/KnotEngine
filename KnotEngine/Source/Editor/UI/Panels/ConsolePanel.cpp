#include "UI/Panels/ConsolePanel.h"

#include <imgui.h>
#include <cstring>
#include <format>
#include <utility>

#include "Core/Log.h"

void FConsolePanel::Startup()
{
	FDebug::SetLogSink(&FConsolePanel::ReceiveLog, this);
}

void FConsolePanel::Shutdown()
{
	FDebug::SetLogSink(nullptr, nullptr);
	std::scoped_lock Lock(MessageMutex);
	Messages.clear();
}

void FConsolePanel::Draw()
{
	if (!ImGui::Begin("Console"))
	{
		ImGui::End();
		return;
	}
	if (ImGui::Button("Clear"))
	{
		std::scoped_lock Lock(MessageMutex);
		Messages.clear();
	}

	KE_LOG_ONCE(Console, Error, "Test Error Message");

	ImGui::SameLine();
	ImGui::SetNextItemWidth(240.0f);
	ImGui::InputTextWithHint("##ConsoleFilter", "Filter", Filter.data(), Filter.size());
	ImGui::Separator();
	ImGui::BeginChild("ConsoleMessages", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);
	{
		std::scoped_lock Lock(MessageMutex);
		for (const FMessage& Message : Messages)
		{
			if (Filter[0] != '\0' && std::strstr(Message.Text.c_str(), Filter.data()) == nullptr)
			{
				continue;
			}
			ImVec4 Color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
			if (Message.Verbosity == ELogVerbosity::Warning)
			{
				Color = ImVec4(1.0f, 0.75f, 0.2f, 1.0f); // Orange
			}
			else if (Message.Verbosity == ELogVerbosity::Error)
			{
				Color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f); // Red
			}
			ImGui::PushStyleColor(ImGuiCol_Text, Color);
			ImGui::TextUnformatted(Message.Text.c_str());
			ImGui::PopStyleColor();
		}
		if (bScrollToBottom)
		{
			ImGui::SetScrollHereY(1.0f);
			bScrollToBottom = false;
		}
	}
	ImGui::EndChild();
	ImGui::End();
}

void FConsolePanel::ReceiveLog(ELogVerbosity Verbosity, std::string_view Category, std::string_view Message, std::string_view File, int Line, void* UserData)
{
	FConsolePanel& Panel = *static_cast<FConsolePanel*>(UserData);
	const char* VerbosityText = "Log";
	if (Verbosity == ELogVerbosity::Warning)
	{
		VerbosityText = "Warning";
	}
	else if (Verbosity == ELogVerbosity::Error)
	{
		VerbosityText = "Error";
	}
	FMessage ConsoleMessage = { Verbosity, FString(File), Line, std::format("[{}] {}: {}", VerbosityText, Category, Message) };

	std::scoped_lock Lock(Panel.MessageMutex);
	static constexpr SIZE_T MaxMessages = 2000;
	if (Panel.Messages.size() == MaxMessages)
	{
		Panel.Messages.erase(Panel.Messages.begin());
	}
	Panel.Messages.push_back(std::move(ConsoleMessage));
	Panel.bScrollToBottom = true;
}
