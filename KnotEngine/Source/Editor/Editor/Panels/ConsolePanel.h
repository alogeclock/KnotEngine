#pragma once

#include "Core/CoreTypes.h"
#include "Core/Debug.h"

#include <mutex>
#include <array>

struct ImGuiInputTextCallbackData;
struct FViewportStatState;

class FConsolePanel
{
public:
	explicit FConsolePanel(FViewportStatState& InViewportStatState);

	void Startup();
	void Shutdown();
	void Draw();
	void RequestCommandInputFocus() { bFocusCommandInput = true; }

private:
	FViewportStatState& ViewportStatState;

	// 자동 완성과 도움말에 사용하는 Console 명령 이름, 설명 및 도움말 그룹이다.
	struct FCommand
	{
		std::string_view Name;
		std::string_view Description;
		std::string_view Group;
	};

	struct FMessage
	{
		ELogVerbosity Verbosity = ELogVerbosity::Display;
		FString File;
		int Line = 0;
		FString Text;
		uint32 Id = 0;
	};

	// 자동 줄바꿈과 새 로그 추가 이후에도 선택 지점을 찾을 수 있도록 원본 메시지의 UTF-8 바이트 위치를 보관한다.
	struct FTextPosition
	{
		uint32 MessageId = 0;
		uint32 ByteOffset = 0;
	};

	// 원본 메시지에서 Panel 폭에 맞게 잘라낸 시각 행이다. Message는 Draw()가 MessageMutex를 소유하는 동안만 유효하다.
	struct FVisibleLine
	{
		const FMessage* Message = nullptr;
		uint32 FirstByte = 0;
		uint32 LastByte = 0;
		bool bEndsPhysicalLine = false;
	};

	static void ReceiveLog(ELogVerbosity Verbosity, std::string_view Category, std::string_view Message, std::string_view File, int Line, void* UserData);
	static int HandleCommandInputCallback(ImGuiInputTextCallbackData* Data);

	static uint32 FindByteOffsetAtMouse(std::string_view Text, float LocalMouseX);
	static uint32 FindWrapByteOffset(std::string_view Text, float WrapWidth);
	static int32 FindVisibleLineIndex(const TArray<FVisibleLine>& VisibleLines, const FTextPosition& Position);

	void DrawToolbar();

	void BuildVisibleLines(TArray<FVisibleLine>& OutVisibleLines, float WrapWidth) const;
	void DrawVisibleLines(const TArray<FVisibleLine>& VisibleLines, float WrapWidth);
	void DrawCommandInput();

	void SubmitCommand(std::string_view Command);
	void SuggestCommand(std::string_view Input);
	void SetCommandInput(std::string_view Command);
	void AddMessage(ELogVerbosity Verbosity, FString Text, FString File = {}, int Line = 0);
	void CopySelection(const TArray<FVisibleLine>& VisibleLines) const;
	void ClearTextSelection();

	// Log sink와 UI가 공유하는 메시지 목록 및 메시지 추가 상태를 보호한다.
	std::mutex MessageMutex;

	TArray<FMessage> Messages; // Console에 보관 중인 원본 로그 메시지 목록
	TArray<FString> CommandHistory;
	TArray<std::string_view> CommandSuggestions;

	inline static constexpr FCommand Commands[] = {
		{ "clear", "Clear console output", "manage console output:" },
		{ "help", "Show available commands", "manage console output:" },
		{ "stat all", "Toggle all viewport statistics", "display viewport statistics:" },
		{ "stat fps", "Toggle viewport FPS statistics", "display viewport statistics:" },
		{ "stat memory", "Toggle viewport memory statistics", "display viewport statistics:" },
	};

	TStaticArray<char, 128> Filter = {};
	TStaticArray<char, 128> CommandInput = {};

	FTextPosition SelectionAnchor;
	FTextPosition SelectionEnd;
	int32 HistoryPosition = -1;
	uint32 NextMessageId = 1;

	bool bSelectingText = false;
	bool bScrollToBottom = false;
	bool bFocusCommandInput = false;
};
