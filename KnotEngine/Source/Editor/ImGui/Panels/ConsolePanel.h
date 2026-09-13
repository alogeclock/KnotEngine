#pragma once

#include "Core/CoreTypes.h"
#include "Core/Debug.h"

#include <mutex>
#include <array>

struct ImGuiInputTextCallbackData;

class FConsolePanel
{
public:
	void Startup();
	void Shutdown();
	void Draw();

private:
	struct FMessage
	{
		ELogVerbosity Verbosity = ELogVerbosity::Log;
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
	static int HandleCommandHistoryCallback(ImGuiInputTextCallbackData* Data);

	static uint32 FindByteOffsetAtMouse(std::string_view Text, float LocalMouseX);
	static uint32 FindWrapByteOffset(std::string_view Text, float WrapWidth);
	static int32 FindVisibleLineIndex(const TArray<FVisibleLine>& VisibleLines, const FTextPosition& Position);

	void DrawToolbar();

	void BuildVisibleLines(TArray<FVisibleLine>& OutVisibleLines, float WrapWidth) const;
	void DrawVisibleLines(const TArray<FVisibleLine>& VisibleLines, float WrapWidth);
	void DrawCommandInput();

	void SubmitCommand(std::string_view Command);
	void AddMessage(ELogVerbosity Verbosity, FString Text, FString File = {}, int Line = 0);
	void CopySelection(const TArray<FVisibleLine>& VisibleLines) const;
	void ClearTextSelection();

	// Log sink와 UI가 공유하는 메시지 목록 및 메시지 추가 상태를 보호한다.
	std::mutex MessageMutex;

	TArray<FMessage> Messages; // Console에 보관 중인 원본 로그 메시지 목록이다.
	TArray<FString> CommandHistory; // 입력된 명령을 제출 순서대로 보관한다.

	TStaticArray<char, 128> Filter = {}; // 메시지 표시 대상을 제한한다.
	TStaticArray<char, 128> CommandInput = {}; // 실행할 Console 명령을 최대 128자로 입력받는다.

	FTextPosition SelectionAnchor;
	FTextPosition SelectionEnd;
	int32 HistoryPosition = -1;
	// 새 메시지에 부여할 ID이며 0은 선택 위치가 없음을 나타내기 위해 예약한다.
	uint32 NextMessageId = 1;
	// 마우스 왼쪽 버튼을 누른 채 텍스트 선택 범위를 갱신하고 있는지 나타낸다.
	bool bSelectingText = false;
	// 새 메시지를 표시한 뒤 다음 Draw에서 출력 하단으로 이동해야 하는지 나타낸다.
	bool bScrollToBottom = false;
};
