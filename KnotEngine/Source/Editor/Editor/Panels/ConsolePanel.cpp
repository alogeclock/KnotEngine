#include "Editor/Panels/ConsolePanel.h"

#include "Editor/Overlays/ViewportStatOverlay.h"

#include <algorithm>
#include <cctype>
#include <imgui.h>
#include <cstring>
#include <format>
#include <utility>

#include "Core/Log.h"

// Viewport 통계 표시 상태를 변경할 수 있도록 Console Panel을 구성한다.
FConsolePanel::FConsolePanel(FViewportStatState& InViewportStatState)
	: ViewportStatState(InViewportStatState)
{
}

// Console 로그 수신기를 등록하고 기존 로그를 받을 준비를 한다.
void FConsolePanel::Startup()
{
	FDebug::SetLogSink(&FConsolePanel::ReceiveLog, this);
}

// 등록한 Console 로그 수신기를 해제한다.
void FConsolePanel::Shutdown()
{
	FDebug::SetLogSink(nullptr, nullptr);
	std::scoped_lock Lock(MessageMutex);
	Messages.clear();
	CommandHistory.clear();
	CommandSuggestions.clear();
	CommandInput.fill('\0');
	HistoryPosition = -1;
	bFocusCommandInput = false;
	ViewportStatState.bShowFPS = false;
	ViewportStatState.bShowMemory = false;
	ClearTextSelection();
}

// 도구 모음과 로그 영역 및 명령 입력창을 순서대로 그린다.
void FConsolePanel::Draw()
{
	if (!ImGui::Begin("Console"))
	{
		ImGui::End();
		return;
	}

	DrawToolbar();

	ImGui::BeginChild("ConsoleMessages", ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing()));
	{
		std::scoped_lock Lock(MessageMutex);
		const float WrapWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);
		TArray<FVisibleLine> VisibleLines;
		BuildVisibleLines(VisibleLines, WrapWidth);
		DrawVisibleLines(VisibleLines, WrapWidth);
	}
	ImGui::EndChild();

	DrawCommandInput();
	ImGui::End();
}

void FConsolePanel::OnOpened()
{
	bFocusCommandInput = true;
}

// Console 필터와 전체 로그 삭제 UI를 그린다.
void FConsolePanel::DrawToolbar()
{
	if (ImGui::Button("Clear"))
	{
		std::scoped_lock Lock(MessageMutex);
		Messages.clear();
		ClearTextSelection();
	}

	ImGui::SameLine();
	ImGui::SetNextItemWidth(240.0f);
	if (ImGui::InputTextWithHint("##ConsoleFilter", "Filter", Filter.data(), Filter.size()))
	{
		ClearTextSelection();
	}
	ImGui::Separator();
}

// 필터와 자동 줄바꿈을 적용하여 이번 프레임에 그릴 시각 행 목록을 구성한다.
void FConsolePanel::BuildVisibleLines(TArray<FVisibleLine>& OutVisibleLines, float WrapWidth) const
{
	static constexpr SIZE_T MaxRenderedLineCount = 1000;
	OutVisibleLines.clear();
	OutVisibleLines.reserve(MaxRenderedLineCount);
	for (auto MessageIterator = Messages.rbegin(); MessageIterator != Messages.rend() && OutVisibleLines.size() < MaxRenderedLineCount; ++MessageIterator)
	{
		const FMessage& Message = *MessageIterator;
		if (Filter[0] != '\0' && std::strstr(Message.Text.c_str(), Filter.data()) == nullptr)
		{
			continue;
		}

		TArray<FVisibleLine> MessageVisibleLines;
		const uint32 TextByteCount = static_cast<uint32>(Message.Text.size());
		uint32 LineStart = 0;
		do
		{
			uint32 LineEnd = LineStart;
			while (LineEnd < TextByteCount && Message.Text[LineEnd] != '\n')
			{
				++LineEnd;
			}
			uint32 RenderEnd = LineEnd;
			if (RenderEnd > LineStart && Message.Text[RenderEnd - 1] == '\r')
			{
				--RenderEnd;
			}

			uint32 FirstByte = LineStart;
			do
			{
				const uint32 RemainingByteCount = RenderEnd - FirstByte;
				const uint32 WrappedByteCount = FindWrapByteOffset(std::string_view(Message.Text).substr(FirstByte, RemainingByteCount), WrapWidth);
				const uint32 LastByte = FirstByte + std::min(RemainingByteCount, WrappedByteCount);
				MessageVisibleLines.push_back({ &Message, FirstByte, LastByte, LastByte == RenderEnd });
				FirstByte = LastByte;
			}
			while (FirstByte < RenderEnd);

			LineStart = LineEnd < TextByteCount ? LineEnd + 1 : TextByteCount;
		}
		while (LineStart < TextByteCount);

		for (auto LineIterator = MessageVisibleLines.rbegin(); LineIterator != MessageVisibleLines.rend() && OutVisibleLines.size() < MaxRenderedLineCount; ++LineIterator)
		{
			OutVisibleLines.push_back(*LineIterator);
		}
	}
	std::reverse(OutVisibleLines.begin(), OutVisibleLines.end());
}

// 시각 행의 텍스트와 드래그 선택 영역 및 Context Menu를 그린다.
void FConsolePanel::DrawVisibleLines(const TArray<FVisibleLine>& VisibleLines, float WrapWidth)
{
	int32 AnchorVisibleIndex = FindVisibleLineIndex(VisibleLines, SelectionAnchor);
	int32 EndVisibleIndex = FindVisibleLineIndex(VisibleLines, SelectionEnd);
	const bool bHasStoredSelection = SelectionAnchor.MessageId != 0 || SelectionEnd.MessageId != 0;
	if (bHasStoredSelection && (AnchorVisibleIndex == -1 || EndVisibleIndex == -1))
	{
		ClearTextSelection();
		AnchorVisibleIndex = -1;
		EndVisibleIndex = -1;
	}

	bool bHoveredLine = false;
	const ImVec2 OutputPosition = ImGui::GetWindowPos();
	const ImVec2 OutputSize = ImGui::GetWindowSize();
	const float OutputTop = OutputPosition.y + ImGui::GetStyle().WindowPadding.y;
	const float OutputBottom = OutputPosition.y + OutputSize.y - ImGui::GetStyle().WindowPadding.y;
	const float SelectionMouseY = std::clamp(ImGui::GetIO().MousePos.y, OutputTop, OutputBottom - 1.0f);
	for (int32 VisibleLineIndex = 0; VisibleLineIndex < static_cast<int32>(VisibleLines.size()); ++VisibleLineIndex)
	{
		const FVisibleLine& VisibleLine = VisibleLines[VisibleLineIndex];
		const FMessage& Message = *VisibleLine.Message;
		const char* const TextBegin = Message.Text.data() + VisibleLine.FirstByte;
		const char* const TextEnd = Message.Text.data() + VisibleLine.LastByte;
		ImVec4 Color(1.0f, 1.0f, 1.0f, 1.0f);
		if (Message.Verbosity == ELogVerbosity::Log)
		{
			Color = ImVec4(0.35f, 0.75f, 1.0f, 1.0f); // Sky Blue
		}
		else if (Message.Verbosity == ELogVerbosity::Warning)
		{
			Color = ImVec4(1.0f, 0.75f, 0.2f, 1.0f); // Orange
		}
		else if (Message.Verbosity == ELogVerbosity::Error)
		{
			Color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f); // Red
		}

		ImGui::PushID(static_cast<int>(Message.Id));
		ImGui::PushID(static_cast<int>(VisibleLine.FirstByte));
		const ImVec2 TextPosition = ImGui::GetCursorScreenPos();
		ImGui::InvisibleButton("##ConsoleLine", ImVec2(WrapWidth, ImGui::GetTextLineHeight()));
		const bool bLineHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		const bool bDraggingOverLine = bSelectingText && ImGui::IsMouseDown(ImGuiMouseButton_Left)
			&& SelectionMouseY >= TextPosition.y && SelectionMouseY < TextPosition.y + ImGui::GetTextLineHeightWithSpacing();
		bHoveredLine |= bLineHovered;
		if (bLineHovered)
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
		}
		if (bLineHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			const uint32 ByteOffset = VisibleLine.FirstByte
				+ FindByteOffsetAtMouse(std::string_view(TextBegin, VisibleLine.LastByte - VisibleLine.FirstByte), ImGui::GetIO().MousePos.x - TextPosition.x);
			SelectionAnchor = { Message.Id, ByteOffset };
			SelectionEnd = SelectionAnchor;
			AnchorVisibleIndex = VisibleLineIndex;
			EndVisibleIndex = VisibleLineIndex;
			bSelectingText = true;
			bScrollToBottom = false;
		}
		else if (bDraggingOverLine)
		{
			const uint32 ByteOffset = VisibleLine.FirstByte
				+ FindByteOffsetAtMouse(std::string_view(TextBegin, VisibleLine.LastByte - VisibleLine.FirstByte), ImGui::GetIO().MousePos.x - TextPosition.x);
			SelectionEnd = { Message.Id, ByteOffset };
			EndVisibleIndex = VisibleLineIndex;
		}

		if (AnchorVisibleIndex != -1 && EndVisibleIndex != -1)
		{
			const int32 FirstSelectedLine = std::min(AnchorVisibleIndex, EndVisibleIndex);
			const int32 LastSelectedLine = std::max(AnchorVisibleIndex, EndVisibleIndex);
			if (VisibleLineIndex >= FirstSelectedLine && VisibleLineIndex <= LastSelectedLine)
			{
				uint32 FirstByte = VisibleLine.FirstByte;
				uint32 LastByte = VisibleLine.LastByte;
				if (AnchorVisibleIndex == EndVisibleIndex)
				{
					FirstByte = std::min(SelectionAnchor.ByteOffset, SelectionEnd.ByteOffset);
					LastByte = std::max(SelectionAnchor.ByteOffset, SelectionEnd.ByteOffset);
				}
				else if (VisibleLineIndex == AnchorVisibleIndex)
				{
					if (AnchorVisibleIndex < EndVisibleIndex)
					{
						FirstByte = SelectionAnchor.ByteOffset;
					}
					else
					{
						LastByte = SelectionAnchor.ByteOffset;
					}
				}
				else if (VisibleLineIndex == EndVisibleIndex)
				{
					if (EndVisibleIndex < AnchorVisibleIndex)
					{
						FirstByte = SelectionEnd.ByteOffset;
					}
					else
					{
						LastByte = SelectionEnd.ByteOffset;
					}
				}
				if (FirstByte != LastByte)
				{
					const float SelectionStartX = TextPosition.x + ImGui::CalcTextSize(TextBegin, Message.Text.data() + FirstByte).x;
					const float SelectionEndX = TextPosition.x + ImGui::CalcTextSize(TextBegin, Message.Text.data() + LastByte).x;
					ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(SelectionStartX, TextPosition.y),
						ImVec2(SelectionEndX, TextPosition.y + ImGui::GetTextLineHeight()), ImGui::GetColorU32(ImGuiCol_TextSelectedBg));
				}
			}
		}

		ImGui::GetWindowDrawList()->AddText(TextPosition, ImGui::ColorConvertFloat4ToU32(Color), TextBegin, TextEnd);
		if (ImGui::BeginPopupContextItem("ConsoleSelectionContext"))
		{
			const bool bCanCopy = AnchorVisibleIndex != -1 && EndVisibleIndex != -1
				&& (AnchorVisibleIndex != EndVisibleIndex || SelectionAnchor.ByteOffset != SelectionEnd.ByteOffset);
			if (ImGui::MenuItem("Copy", nullptr, false, bCanCopy))
			{
				CopySelection(VisibleLines);
			}
			ImGui::EndPopup();
		}
		ImGui::PopID();
		ImGui::PopID();
	}

	if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !bHoveredLine)
	{
		ClearTextSelection();
	}
	if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		bSelectingText = false;
	}
	else if (bSelectingText)
	{
		static constexpr float SelectionScrollSpeed = 600.0f;
		if (ImGui::GetIO().MousePos.y < OutputTop)
		{
			ImGui::SetScrollY(std::max(0.0f, ImGui::GetScrollY() - SelectionScrollSpeed * ImGui::GetIO().DeltaTime));
		}
		else if (ImGui::GetIO().MousePos.y > OutputBottom)
		{
			ImGui::SetScrollY(std::min(ImGui::GetScrollMaxY(), ImGui::GetScrollY() + SelectionScrollSpeed * ImGui::GetIO().DeltaTime));
		}
	}
	const bool bHasSelection = AnchorVisibleIndex != -1 && EndVisibleIndex != -1
		&& (AnchorVisibleIndex != EndVisibleIndex || SelectionAnchor.ByteOffset != SelectionEnd.ByteOffset);
	if (bHasSelection && ImGui::IsWindowFocused() && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false))
	{
		CopySelection(VisibleLines);
	}
	if (bScrollToBottom)
	{
		ImGui::SetScrollHereY(1.0f);
		bScrollToBottom = false;
	}
}

// 자동 완성과 명령 기록 탐색을 지원하는 Console 입력창을 그리고 제출된 명령을 처리한다.
void FConsolePanel::DrawCommandInput()
{
	if (bFocusCommandInput)
	{
		ImGui::SetKeyboardFocusHere();
		bFocusCommandInput = false;
	}

	ImGui::SetNextItemWidth(-1.0f);
	const ImGuiInputTextFlags InputFlags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_CallbackEdit;
	const bool bSubmitted = ImGui::InputTextWithHint(
		"##ConsoleCommand",
		"Enter command",
		CommandInput.data(),
		CommandInput.size(),
		InputFlags,
		&FConsolePanel::HandleCommandInputCallback,
		this);
	const bool bInputActive = ImGui::IsItemActive();
	const ImVec2 InputMin = ImGui::GetItemRectMin();
	const ImVec2 InputMax = ImGui::GetItemRectMax();
	if (bInputActive && !CommandSuggestions.empty())
	{
		ImGui::OpenPopup("##ConsoleCommandSuggestions");
	}

	ImGui::SetNextWindowPos(InputMin, ImGuiCond_Always, ImVec2(0.0f, 1.0f));
	ImGui::SetNextWindowSizeConstraints(ImVec2(InputMax.x - InputMin.x, 0.0f), ImVec2(InputMax.x - InputMin.x, 180.0f));
	constexpr ImGuiWindowFlags SuggestionFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNavFocus;
	if (ImGui::BeginPopup("##ConsoleCommandSuggestions", SuggestionFlags))
	{
		if (!bInputActive || CommandSuggestions.empty())
		{
			ImGui::CloseCurrentPopup();
		}
		else
		{
			for (const std::string_view Suggestion : CommandSuggestions)
			{
				if (ImGui::Selectable(Suggestion.data()))
				{
					SetCommandInput(Suggestion);
					CommandSuggestions.clear();
					bFocusCommandInput = true;
					ImGui::CloseCurrentPopup();
				}
			}
		}
		ImGui::EndPopup();
	}

	if (bSubmitted)
	{
		SubmitCommand(CommandInput.data());
		CommandInput.fill('\0');
		CommandSuggestions.clear();
		ImGui::SetKeyboardFocusHere(-1);
	}
}

// 시각 행 안의 마우스 X 좌표를 가장 가까운 UTF-8 문자 경계의 바이트 위치로 변환한다.
uint32 FConsolePanel::FindByteOffsetAtMouse(std::string_view Text, float LocalMouseX)
{
	if (LocalMouseX <= 0.0f)
	{
		return 0;
	}

	float TextWidth = 0.0f;
	for (uint32 ByteOffset = 0; ByteOffset < Text.size();)
	{
		const unsigned char FirstByte = static_cast<unsigned char>(Text[ByteOffset]);
		uint32 CharacterSize = 1;
		if ((FirstByte & 0xf0) == 0xf0)
		{
			CharacterSize = 4;
		}
		else if ((FirstByte & 0xe0) == 0xe0)
		{
			CharacterSize = 3;
		}
		else if ((FirstByte & 0xc0) == 0xc0)
		{
			CharacterSize = 2;
		}
		CharacterSize = std::min(CharacterSize, static_cast<uint32>(Text.size()) - ByteOffset);

		const float CharacterWidth = ImGui::CalcTextSize(Text.data() + ByteOffset, Text.data() + ByteOffset + CharacterSize).x;
		if (LocalMouseX < TextWidth + CharacterWidth * 0.5f)
		{
			return ByteOffset;
		}
		TextWidth += CharacterWidth;
		ByteOffset += CharacterSize;
	}
	return static_cast<uint32>(Text.size());
}

// UTF-8 문자를 나누지 않으면서 지정한 폭에 들어가는 최대 바이트 수를 계산한다.
uint32 FConsolePanel::FindWrapByteOffset(std::string_view Text, float WrapWidth)
{
	float TextWidth = 0.0f;
	for (uint32 ByteOffset = 0; ByteOffset < Text.size();)
	{
		const unsigned char FirstByte = static_cast<unsigned char>(Text[ByteOffset]);
		uint32 CharacterSize = 1;
		if ((FirstByte & 0xf0) == 0xf0)
		{
			CharacterSize = 4;
		}
		else if ((FirstByte & 0xe0) == 0xe0)
		{
			CharacterSize = 3;
		}
		else if ((FirstByte & 0xc0) == 0xc0)
		{
			CharacterSize = 2;
		}
		CharacterSize = std::min(CharacterSize, static_cast<uint32>(Text.size()) - ByteOffset);

		const float CharacterWidth = ImGui::CalcTextSize(Text.data() + ByteOffset, Text.data() + ByteOffset + CharacterSize).x;
		if (ByteOffset > 0 && TextWidth + CharacterWidth > WrapWidth)
		{
			return ByteOffset;
		}
		TextWidth += CharacterWidth;
		ByteOffset += CharacterSize;
	}
	return static_cast<uint32>(Text.size());
}

// 원본 메시지의 텍스트 위치를 포함하는 현재 시각 행의 인덱스를 찾는다.
int32 FConsolePanel::FindVisibleLineIndex(const TArray<FVisibleLine>& VisibleLines, const FTextPosition& Position)
{
	for (int32 LineIndex = 0; LineIndex < static_cast<int32>(VisibleLines.size()); ++LineIndex)
	{
		const FVisibleLine& VisibleLine = VisibleLines[LineIndex];
		if (VisibleLine.Message->Id != Position.MessageId)
		{
			continue;
		}
		const bool bEmptyLine = VisibleLine.FirstByte == VisibleLine.LastByte && Position.ByteOffset == VisibleLine.FirstByte;
		const bool bInsideLine = Position.ByteOffset >= VisibleLine.FirstByte && Position.ByteOffset < VisibleLine.LastByte;
		const bool bAtPhysicalLineEnd = Position.ByteOffset == VisibleLine.LastByte && VisibleLine.bEndsPhysicalLine;
		if (bEmptyLine || bInsideLine || bAtPhysicalLineEnd)
		{
			return LineIndex;
		}
	}
	return -1;
}

// 자동 줄바꿈은 제외하고 원본 개행만 유지한 선택 문자열을 클립보드에 복사한다.
void FConsolePanel::CopySelection(const TArray<FVisibleLine>& VisibleLines) const
{
	int32 FirstLine = FindVisibleLineIndex(VisibleLines, SelectionAnchor);
	int32 LastLine = FindVisibleLineIndex(VisibleLines, SelectionEnd);
	if (FirstLine == -1 || LastLine == -1)
	{
		return;
	}

	uint32 FirstByte = SelectionAnchor.ByteOffset;
	uint32 LastByte = SelectionEnd.ByteOffset;
	if (FirstLine > LastLine || (FirstLine == LastLine && FirstByte > LastByte))
	{
		std::swap(FirstLine, LastLine);
		std::swap(FirstByte, LastByte);
	}

	FString SelectedText;
	for (int32 LineIndex = FirstLine; LineIndex <= LastLine; ++LineIndex)
	{
		const FVisibleLine& VisibleLine = VisibleLines[LineIndex];
		const FString& Text = VisibleLine.Message->Text;
		const uint32 CopyStart = LineIndex == FirstLine ? FirstByte : VisibleLine.FirstByte;
		const uint32 CopyEnd = LineIndex == LastLine ? LastByte : VisibleLine.LastByte;
		SelectedText.append(Text.data() + CopyStart, CopyEnd - CopyStart);
		if (LineIndex != LastLine)
		{
			const FVisibleLine& NextVisibleLine = VisibleLines[LineIndex + 1];
			const bool bContinuesWrappedLine = VisibleLine.Message == NextVisibleLine.Message && VisibleLine.LastByte == NextVisibleLine.FirstByte;
			if (!bContinuesWrappedLine)
			{
				SelectedText.push_back('\n');
			}
		}
	}
	ImGui::SetClipboardText(SelectedText.c_str());
}

// 텍스트 선택의 시작과 끝 및 진행 중인 드래그 상태를 초기화한다.
void FConsolePanel::ClearTextSelection()
{
	SelectionAnchor = {};
	SelectionEnd = {};
	bSelectingText = false;
}

// 문자 편집 시 자동 완성을 갱신하고 위·아래 방향키로 Console 명령 기록을 탐색한다.
int FConsolePanel::HandleCommandInputCallback(ImGuiInputTextCallbackData* Data)
{
	FConsolePanel& Panel = *static_cast<FConsolePanel*>(Data->UserData);
	if (Data->EventFlag == ImGuiInputTextFlags_CallbackEdit)
	{
		Panel.SuggestCommand(std::string_view(Data->Buf, static_cast<size_t>(Data->BufTextLen)));
		return 0;
	}
	if (Data->EventFlag != ImGuiInputTextFlags_CallbackHistory)
	{
		return 0;
	}

	const int32 PreviousPosition = Panel.HistoryPosition;
	if (Data->EventKey == ImGuiKey_UpArrow)
	{
		if (Panel.HistoryPosition == -1)
		{
			Panel.HistoryPosition = static_cast<int32>(Panel.CommandHistory.size()) - 1;
		}
		else if (Panel.HistoryPosition > 0)
		{
			--Panel.HistoryPosition;
		}
	}
	else if (Data->EventKey == ImGuiKey_DownArrow && Panel.HistoryPosition != -1)
	{
		if (++Panel.HistoryPosition >= static_cast<int32>(Panel.CommandHistory.size()))
		{
			Panel.HistoryPosition = -1;
		}
	}

	if (Panel.HistoryPosition != PreviousPosition)
	{
		const char* HistoryText = Panel.HistoryPosition >= 0 ? Panel.CommandHistory[Panel.HistoryPosition].c_str() : "";
		Data->DeleteChars(0, Data->BufTextLen);
		Data->InsertChars(0, HistoryText);
		Panel.SuggestCommand(std::string_view(Data->Buf, static_cast<size_t>(Data->BufTextLen)));
	}
	return 0;
}

// 입력 명령을 정규화하고 기록한 뒤 Console이 직접 지원하는 명령을 실행한다.
void FConsolePanel::SubmitCommand(std::string_view Command)
{
	FString TrimmedCommand(Command);
	const auto FirstCharacter = std::find_if_not(TrimmedCommand.begin(), TrimmedCommand.end(), [](unsigned char Character)
	{
		return std::isspace(Character) != 0;
	});
	const auto LastCharacter = std::find_if_not(TrimmedCommand.rbegin(), TrimmedCommand.rend(), [](unsigned char Character)
	{
		return std::isspace(Character) != 0;
	}).base();
	if (FirstCharacter >= LastCharacter)
	{
		return;
	}
	TrimmedCommand = FString(FirstCharacter, LastCharacter);

	static constexpr size_t MaxHistoryCount = 100;
	if (CommandHistory.empty() || CommandHistory.back() != TrimmedCommand)
	{
		if (CommandHistory.size() == MaxHistoryCount)
		{
			CommandHistory.erase(CommandHistory.begin());
		}
		CommandHistory.push_back(TrimmedCommand);
	}
	HistoryPosition = -1;

	FString NormalizedCommand = TrimmedCommand;
	std::transform(NormalizedCommand.begin(), NormalizedCommand.end(), NormalizedCommand.begin(), [](unsigned char Character)
	{
		return static_cast<char>(std::tolower(Character));
	});

	if (NormalizedCommand == "clear")
	{
		std::scoped_lock Lock(MessageMutex);
		Messages.clear();
		ClearTextSelection();
		bScrollToBottom = false;
		return;
	}

	AddMessage(ELogVerbosity::Display, "> " + TrimmedCommand);
	if (NormalizedCommand == "help")
	{
		FString HelpText;
		std::string_view PreviousGroup;
		for (const FCommand& CommandInfo : Commands)
		{
			if (CommandInfo.Group != PreviousGroup)
			{
				if (!HelpText.empty())
				{
					HelpText += "\n\n";
				}
				HelpText += CommandInfo.Group;
				PreviousGroup = CommandInfo.Group;
			}
			HelpText += std::format("\n      {:<13} {}", CommandInfo.Name, CommandInfo.Description);
		}
		AddMessage(ELogVerbosity::Display, std::move(HelpText));
	}
	else if (NormalizedCommand == "stat all")
	{
		const bool bEnableAllStats = !ViewportStatState.bShowFPS || !ViewportStatState.bShowMemory;
		ViewportStatState.bShowFPS = bEnableAllStats;
		ViewportStatState.bShowMemory = bEnableAllStats;
		AddMessage(ELogVerbosity::Display, bEnableAllStats ? "All viewport statistics enabled" : "All viewport statistics disabled");
	}
	else if (NormalizedCommand == "stat fps")
	{
		ViewportStatState.bShowFPS = !ViewportStatState.bShowFPS;
		AddMessage(ELogVerbosity::Display, ViewportStatState.bShowFPS ? "Viewport FPS statistics enabled" : "Viewport FPS statistics disabled");
	}
	else if (NormalizedCommand == "stat memory")
	{
		ViewportStatState.bShowMemory = !ViewportStatState.bShowMemory;
		AddMessage(ELogVerbosity::Display, ViewportStatState.bShowMemory ? "Viewport memory statistics enabled" : "Viewport memory statistics disabled");
	}
	else
	{
		AddMessage(ELogVerbosity::Display, "Unknown command: " + TrimmedCommand);
	}
}

// 입력 문자열을 prefix로 갖는 명령만 사전식 명령 목록에서 찾아 캐시한다.
void FConsolePanel::SuggestCommand(std::string_view Input)
{
	CommandSuggestions.clear();
	if (Input.empty())
	{
		return;
	}

	FString NormalizedInput(Input);
	std::transform(NormalizedInput.begin(), NormalizedInput.end(), NormalizedInput.begin(), [](unsigned char Character)
	{
		return static_cast<char>(std::tolower(Character));
	});
	for (const FCommand& CommandInfo : Commands)
	{
		if (CommandInfo.Name.starts_with(NormalizedInput))
		{
			CommandSuggestions.push_back(CommandInfo.Name);
		}
	}
}

// 선택한 자동 완성 명령을 입력 버퍼 크기에 맞춰 복사한다.
void FConsolePanel::SetCommandInput(std::string_view Command)
{
	CommandInput.fill('\0');
	const size_t CommandLength = std::min(Command.size(), CommandInput.size() - 1);
	std::memcpy(CommandInput.data(), Command.data(), CommandLength);
}

// 원본 로그 문자열을 MessageMutex로 보호되는 제한된 메시지 목록에 추가한다.
void FConsolePanel::AddMessage(ELogVerbosity Verbosity, FString Text, FString File, int Line)
{
	FMessage ConsoleMessage;
	ConsoleMessage.Verbosity = Verbosity;
	ConsoleMessage.File = std::move(File);
	ConsoleMessage.Line = Line;
	ConsoleMessage.Text = std::move(Text);

	std::scoped_lock Lock(MessageMutex);
	ConsoleMessage.Id = NextMessageId++;
	static constexpr SIZE_T MaxMessages = 2000;
	if (Messages.size() == MaxMessages)
	{
		Messages.erase(Messages.begin());
	}
	Messages.push_back(std::move(ConsoleMessage));
	bScrollToBottom = true;
}

// FDebug sink로 전달된 로그를 Console 표시 형식으로 구성하여 해당 Panel에 전달한다.
void FConsolePanel::ReceiveLog(ELogVerbosity Verbosity, std::string_view Category, std::string_view Message, std::string_view File, int Line, void* UserData)
{
	FConsolePanel& Panel = *static_cast<FConsolePanel*>(UserData);
	const char* VerbosityText = "Display";
	if (Verbosity == ELogVerbosity::Log)
	{
		VerbosityText = "Log";
	}
	else if (Verbosity == ELogVerbosity::Warning)
	{
		VerbosityText = "Warning";
	}
	else if (Verbosity == ELogVerbosity::Error)
	{
		VerbosityText = "Error";
	}
	Panel.AddMessage(Verbosity, std::format("[{}] {}: {}", VerbosityText, Category, Message), FString(File), Line);
}
