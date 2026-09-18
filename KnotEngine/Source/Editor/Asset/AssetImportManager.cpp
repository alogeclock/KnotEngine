#include "Asset/AssetImportManager.h"

#include "Core/Assert.h"

#include <Windows.h>
#include <objbase.h>
#include <utility>

FAssetImportManager::~FAssetImportManager()
{
	Shutdown();
}

// GLB Import 요청을 처리할 단일 Worker Thread를 시작한다.
void FAssetImportManager::Startup()
{
	check(!Worker.joinable());
	Worker = std::jthread([this](std::stop_token StopToken)
	{
		WorkerLoop(StopToken);
	});
}

// 대기 중인 요청을 취소하고 실행 중인 Import가 끝날 때까지 Worker Thread를 정리한다.
void FAssetImportManager::Shutdown()
{
	if (!Worker.joinable())
	{
		return;
	}

	Worker.request_stop();
	Condition.notify_all();
	Worker.join();

	std::scoped_lock Lock(Mutex);
	PendingRequests = TQueue<FRequest>();
	CompletedRequests.clear();
	ActiveSourceKeys.clear();
	CurrentSourceFilePath.clear();
	bImportRunning = false;
}

// Bottom Toolbar가 표시할 현재 Import와 대기 Queue 상태를 복사한다.
FAssetImportStatus FAssetImportManager::GetStatus() const
{
	std::scoped_lock Lock(Mutex);
	FAssetImportStatus Status;
	Status.SourceFilePath = CurrentSourceFilePath;
	Status.QueuedCount = PendingRequests.size();
	Status.bRunning = bImportRunning;
	if (bImportRunning)
	{
		Status.ElapsedSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - CurrentStartTime).count();
	}
	return Status;
}

// 같은 Source의 중복 실행을 막고 GLB Import 요청을 Worker Queue에 추가한다.
bool FAssetImportManager::EnqueueGLB(const std::filesystem::path& SourceFilePath, const FString& DestinationAssetPath)
{
	if (!Worker.joinable() || SourceFilePath.empty() || DestinationAssetPath.empty())
	{
		return false;
	}

	const FWString SourceKey = MakeSourceKey(SourceFilePath);
	{
		std::scoped_lock Lock(Mutex);
		if (ActiveSourceKeys.contains(SourceKey))
		{
			return false;
		}
		ActiveSourceKeys.emplace(SourceKey);
		PendingRequests.push({ SourceFilePath, DestinationAssetPath });
	}
	Condition.notify_one();
	return true;
}

// 완료된 Import 결과를 Main Thread로 옮기고 해당 Source를 다시 요청할 수 있게 한다.
void FAssetImportManager::DrainCompleted(TArray<FAssetImportCompletion>& OutCompletions)
{
	std::scoped_lock Lock(Mutex);
	OutCompletions.reserve(OutCompletions.size() + CompletedRequests.size());
	for (FAssetImportCompletion& Completed : CompletedRequests)
	{
		ActiveSourceKeys.erase(MakeSourceKey(Completed.SourceFilePath));
		OutCompletions.push_back(std::move(Completed));
	}
	CompletedRequests.clear();
}

// 대기 중이거나 Worker Thread에서 처리 중인 Import가 있는지 확인한다.
bool FAssetImportManager::HasActiveImports() const
{
	std::scoped_lock Lock(Mutex);
	return !ActiveSourceKeys.empty();
}

// Source GLB가 대기 중이거나 Worker Thread에서 처리 중인지 확인한다.
bool FAssetImportManager::IsImporting(const std::filesystem::path& SourceFilePath) const
{
	std::scoped_lock Lock(Mutex);
	return ActiveSourceKeys.contains(MakeSourceKey(SourceFilePath));
}

// 동일한 Source 경로를 하나의 Import 요청으로 식별할 수 있도록 정규화한다.
FWString FAssetImportManager::MakeSourceKey(const std::filesystem::path& SourceFilePath)
{
	return SourceFilePath.lexically_normal().generic_wstring();
}

// Queue의 GLB를 순차 Import하고 완료 결과만 Main Thread가 소비할 Queue에 기록한다.
void FAssetImportManager::WorkerLoop(std::stop_token StopToken)
{
	const HRESULT InitializeResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	const bool bUninitialize = SUCCEEDED(InitializeResult);
	while (!StopToken.stop_requested())
	{
		FRequest Request;
		{
			std::unique_lock Lock(Mutex);
			Condition.wait(Lock, [this, StopToken]
			{
				return StopToken.stop_requested() || !PendingRequests.empty();
			});
			if (StopToken.stop_requested())
			{
				break;
			}
			Request = std::move(PendingRequests.front());
			PendingRequests.pop();
			CurrentSourceFilePath = Request.SourceFilePath;
			CurrentStartTime = std::chrono::steady_clock::now();
			bImportRunning = true;
		}

		FAssetImportResult Result;
		if (FAILED(InitializeResult))
		{
			Result.Error = "Asset Import Worker의 COM 초기화에 실패했다.";
		}
		else
		{
			Result = FAssetImporter().ImportGLB(Request.SourceFilePath, Request.DestinationAssetPath);
		}

		std::scoped_lock Lock(Mutex);
		CompletedRequests.push_back({ std::move(Request.SourceFilePath), std::move(Result) });
		CurrentSourceFilePath.clear();
		bImportRunning = false;
	}

	if (bUninitialize)
	{
		CoUninitialize();
	}
}
