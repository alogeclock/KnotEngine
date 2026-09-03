#include "Core/Debug.h"

#include "Core/IO/Paths.h"

#include <Windows.h>
#include <filesystem>
#include <intrin.h>
#include <mutex>
#include <system_error>

struct FDebug::FState
{
	HANDLE LogFileHandle = INVALID_HANDLE_VALUE;
	std::mutex Mutex;
	bool bFileWriteFailureReported = false;
};

FDebug::FState& FDebug::GetState()
{
	static FState State;
	return State;
}

void FDebug::CheckFailed(const FDebugContext& Context)
{
	ReportFailure("Check", Context, {});
}

void FDebug::PanicFailed(const FDebugContext& Context)
{
	ReportFailure("Panic", Context, {});
}

void FDebug::Startup()
{
	FState& State = GetState();
	std::scoped_lock Lock(State.Mutex);

	if (State.LogFileHandle != INVALID_HANDLE_VALUE)
	{
		return;
	}
	State.bFileWriteFailureReported = false;

	const FWString LogDirectory = FPaths::LogDir();
	std::error_code ErrorCode;
	std::filesystem::create_directories(LogDirectory, ErrorCode);
	if (ErrorCode)
	{
		OutputDebugStringA("KnotEngine 로그 디렉터리를 생성하지 못했습니다.\n");
		return;
	}

	const FWString LogPath = LogDirectory + L"KnotEngine.log";
	State.LogFileHandle = CreateFileW(
	    LogPath.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
	    FILE_ATTRIBUTE_NORMAL, nullptr);
	if (State.LogFileHandle == INVALID_HANDLE_VALUE)
	{
		OutputDebugStringA("KnotEngine 로그 파일을 생성하지 못했습니다.\n");
	}
}

void FDebug::Shutdown()
{
	FState& State = GetState();
	std::scoped_lock Lock(State.Mutex);

	if (State.LogFileHandle == INVALID_HANDLE_VALUE)
	{
		return;
	}

	FlushFileBuffers(State.LogFileHandle);
	CloseHandle(State.LogFileHandle);
	State.LogFileHandle = INVALID_HANDLE_VALUE;
}

void FDebug::Flush()
{
	FState& State = GetState();
	std::scoped_lock Lock(State.Mutex);

	if (State.LogFileHandle != INVALID_HANDLE_VALUE)
	{
		FlushFileBuffers(State.LogFileHandle);
	}
}

bool FDebug::IsDebuggerAttached()
{
	return ::IsDebuggerPresent() != FALSE;
}

[[noreturn]] void FDebug::Fatal()
{
	__fastfail(FAST_FAIL_FATAL_APP_EXIT);
}

const char* FDebug::ToString(ELogVerbosity Verbosity)
{
	switch (Verbosity)
	{
	case ELogVerbosity::Log: return "Log";
	case ELogVerbosity::Warning: return "Warning";
	case ELogVerbosity::Error: return "Error";
	}
	return "Log";
}

void FDebug::WriteMessage(const char* Message, size_t Length, bool bFlush)
{
	FState& State = GetState();
	std::scoped_lock Lock(State.Mutex);

	OutputDebugStringA(Message);

	if (State.LogFileHandle == INVALID_HANDLE_VALUE)
	{
		return;
	}

	DWORD WrittenLength = 0;
	const BOOL bWriteSucceeded = WriteFile(State.LogFileHandle, Message, static_cast<DWORD>(Length), &WrittenLength, nullptr);
	if (!bWriteSucceeded || static_cast<size_t>(WrittenLength) != Length)
	{
		if (!State.bFileWriteFailureReported)
		{
			State.bFileWriteFailureReported = true;
			OutputDebugStringA("KnotEngine 로그 파일 쓰기에 실패했습니다.\n");
		}
		return;
	}

	if (bFlush)
	{
		FlushFileBuffers(State.LogFileHandle);
	}
}

void FDebug::ReportFailure(const char* Type, const FDebugContext& Context, std::string_view Message)
{
	char Buffer[BufferCapacity];
	const auto Result = std::format_to_n(Buffer, BufferCapacity - 1, "{}({}): [{} Failed] {} | {}{}{}\n",
	                                     Context.File ? Context.File : "?",
	                                     Context.Line,
	                                     Type,
	                                     Context.Expression ? Context.Expression : "?",
	                                     Context.Function ? Context.Function : "?",
	                                     Message.empty() ? "" : " - ",
	                                     Message);
	*Result.out = '\0';

	WriteMessage(Buffer, static_cast<size_t>(Result.out - Buffer), true);
}
