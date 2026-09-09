#pragma once

#include "Core/CoreTypes.h"
#include "Core/Debug.h"

#include <mutex>
#include <array>

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
	};

	static void ReceiveLog(ELogVerbosity Verbosity, std::string_view Category, std::string_view Message, std::string_view File, int Line, void* UserData);

	std::mutex MessageMutex;
	TArray<FMessage> Messages;
	TStaticArray<char, 128> Filter = {};
	bool bScrollToBottom = false;
};
