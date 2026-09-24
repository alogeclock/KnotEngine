#pragma once

#include "EngineAPI.h"
#include "Core/CoreTypes.h"

#include <Windows.h>
#include <chrono>
#include <filesystem>
#include <optional>

// Windows 변경 알림 API를 직접 사용하며, 알림 후 등록된 파일의 실제 바이트 변화를 확인한다.
// 추후 멀티 플랫폼 엔진을 고려할 경우 추상화를 고려한다.
class ENGINE_API FDirectoryWatcher final
{
public:
	struct FChangedFile
	{
		std::filesystem::path Path;
		std::optional<TArray<uint8>> Contents;
	};

	FDirectoryWatcher() = default;
	~FDirectoryWatcher();

	FDirectoryWatcher(const FDirectoryWatcher&) = delete;
	FDirectoryWatcher& operator=(const FDirectoryWatcher&) = delete;

	bool Start(const std::filesystem::path& RootPath);
	void Stop();

	void Watch(const std::filesystem::path& FilePath);
	TArray<FChangedFile> PollChanges();

private:
	struct FWatchedFile
	{
		std::filesystem::path Path;
		std::optional<TArray<uint8>> LastReported;
		std::optional<TArray<uint8>> Pending;
		std::chrono::steady_clock::time_point PendingSince;
		bool bPending = false;
	};

	static std::optional<TArray<uint8>> ReadFile(const std::filesystem::path& FilePath);

	HANDLE ChangeNotification = INVALID_HANDLE_VALUE;
	TMap<std::filesystem::path, FWatchedFile> WatchedFiles;
};
