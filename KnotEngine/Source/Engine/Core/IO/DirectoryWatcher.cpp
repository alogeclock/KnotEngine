#include "Core/IO/DirectoryWatcher.h"

#include "Core/Log.h"

#include <fstream>
#include <limits>

// 감시 객체가 소멸할 때 Windows 변경 알림 Handle을 닫는다.
FDirectoryWatcher::~FDirectoryWatcher()
{
	Stop();
}

// Windows 디렉터리 변경 알림을 열어 하위 폴더까지 감시한다.
bool FDirectoryWatcher::Start(const std::filesystem::path& RootPath)
{
	Stop();
	std::filesystem::path NativeRoot = RootPath.lexically_normal();
	NativeRoot.make_preferred();
	ChangeNotification = FindFirstChangeNotificationW(NativeRoot.c_str(), TRUE,
		FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE);
	return ChangeNotification != INVALID_HANDLE_VALUE;
}

// Windows 변경 알림을 닫고 등록된 파일의 비교 상태를 비운다.
void FDirectoryWatcher::Stop()
{
	if (ChangeNotification != INVALID_HANDLE_VALUE)
	{
		FindCloseChangeNotification(ChangeNotification);
		ChangeNotification = INVALID_HANDLE_VALUE;
	}
	WatchedFiles.clear();
}

// 새 파일만 현재 내용을 기준선으로 등록한다. 이미 등록된 파일은 다시 읽지 않는다.
void FDirectoryWatcher::Watch(const std::filesystem::path& FilePath)
{
	if (ChangeNotification == INVALID_HANDLE_VALUE)
	{
		return;
	}
	const std::filesystem::path NormalizedPath = FilePath.lexically_normal();
	if (WatchedFiles.contains(NormalizedPath))
	{
		return;
	}
	WatchedFiles.emplace(NormalizedPath, FWatchedFile{ NormalizedPath, ReadFile(NormalizedPath) });
}

// 파일을 읽지 못한 상태도 하나의 상태로 보관하여 삭제·복원을 변경으로 감지한다.
std::optional<TArray<uint8>> FDirectoryWatcher::ReadFile(const std::filesystem::path& FilePath)
{
	std::ifstream Stream(FilePath, std::ios::binary | std::ios::ate);
	if (!Stream)
	{
		return std::nullopt;
	}
	const std::streamoff Size = Stream.tellg();
	if (Size < 0 || static_cast<uint64>(Size) > (std::numeric_limits<SIZE_T>::max)())
	{
		return std::nullopt;
	}
	TArray<uint8> Bytes(static_cast<SIZE_T>(Size));
	Stream.seekg(0, std::ios::beg);
	if (!Bytes.empty() && !Stream.read(reinterpret_cast<char*>(Bytes.data()), Size))
	{
		return std::nullopt;
	}
	return Bytes;
}

// 알림 후 내용이 0.5초 동안 같을 때만 확정하고, 실패한 동일 내용은 다시 보고하지 않는다.
TArray<FDirectoryWatcher::FChangedFile> FDirectoryWatcher::PollChanges()
{
	TArray<FChangedFile> Changes;
	if (ChangeNotification == INVALID_HANDLE_VALUE)
	{
		return Changes;
	}
	const DWORD WaitResult = WaitForSingleObject(ChangeNotification, 0);
	const auto Now = std::chrono::steady_clock::now();
	if (WaitResult == WAIT_OBJECT_0)
	{
		if (!FindNextChangeNotification(ChangeNotification))
		{
			KE_LOG(LogDirectoryWatcher, Error, "Directory Watcher 변경 알림을 다시 등록하지 못했다.");
			Stop();
			return Changes;
		}
		for (auto& [Path, File] : WatchedFiles)
		{
			std::optional<TArray<uint8>> Current = ReadFile(Path);
			if (Current == File.LastReported)
			{
				File.bPending = false;
				File.Pending.reset();
			}
			else if (!File.bPending || Current != File.Pending)
			{
				File.Pending = std::move(Current);
				File.PendingSince = Now;
				File.bPending = true;
			}
		}
	}
	else if (WaitResult == WAIT_FAILED)
	{
		KE_LOG(LogDirectoryWatcher, Error, "Directory Watcher 변경 알림을 읽지 못했다.");
		Stop();
		return Changes;
	}
	for (auto& [Path, File] : WatchedFiles)
	{
		if (!File.bPending)
		{
			continue;
		}
		if (Now - File.PendingSince < std::chrono::milliseconds(500))
		{
			continue;
		}
		std::optional<TArray<uint8>> Current = ReadFile(Path);
		if (Current != File.Pending)
		{
			File.Pending = std::move(Current);
			File.PendingSince = Now;
			continue;
		}
		File.bPending = false;
		if (Current != File.LastReported)
		{
			File.LastReported = Current;
			Changes.push_back({ Path, std::move(Current) });
		}
		File.Pending.reset();
	}
	return Changes;
}
