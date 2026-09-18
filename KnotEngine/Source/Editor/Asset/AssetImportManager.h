#pragma once

#include "Asset/AssetImporter.h"
#include "Core/CoreTypes.h"

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <thread>

// Worker Thread에서 끝난 GLB Import 결과와 원본 경로다.
struct FAssetImportCompletion
{
	std::filesystem::path SourceFilePath;
	FAssetImportResult Result;
};

struct FAssetImportStatus
{
	std::filesystem::path SourceFilePath;
	SIZE_T QueuedCount = 0;
	double ElapsedSeconds = 0.0;
	bool bRunning = false;
};

// 비동기 GLB Import 요청과 완료 결과를 단일 Worker Thread에서 처리하는 Editor 관리자다.
class FAssetImportManager final
{
public:
	FAssetImportManager() = default;
	~FAssetImportManager();

	FAssetImportManager(const FAssetImportManager&) = delete;
	FAssetImportManager& operator=(const FAssetImportManager&) = delete;
	FAssetImportManager(FAssetImportManager&&) = delete;
	FAssetImportManager& operator=(FAssetImportManager&&) = delete;

	void Startup();
	void Shutdown();

	bool EnqueueGLB(const std::filesystem::path& SourceFilePath, const FString& DestinationAssetPath);
	void DrainCompleted(TArray<FAssetImportCompletion>& OutCompletions);
	FAssetImportStatus GetStatus() const;
	bool HasActiveImports() const;
	bool IsImporting(const std::filesystem::path& SourceFilePath) const;

private:
	struct FRequest
	{
		std::filesystem::path SourceFilePath;
		FString DestinationAssetPath;
	};

	static FWString MakeSourceKey(const std::filesystem::path& SourceFilePath);
	void WorkerLoop(std::stop_token StopToken);

	mutable std::mutex Mutex;
	std::condition_variable Condition;
	std::jthread Worker;
	TQueue<FRequest> PendingRequests;
	TArray<FAssetImportCompletion> CompletedRequests;
	TSet<FWString> ActiveSourceKeys;
	std::filesystem::path CurrentSourceFilePath;
	std::chrono::steady_clock::time_point CurrentStartTime;
	bool bImportRunning = false;
};
