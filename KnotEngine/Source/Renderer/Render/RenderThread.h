#pragma once

#include "RendererAPI.h"

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <string_view>
#include <thread>

// Render Command를 FIFO로 실행하는 전용 Thread다. GPU API 호출은 이 Thread에서만 수행한다.
class RENDERER_API FRenderThread
{
public:
	FRenderThread() = default;
	~FRenderThread();

	FRenderThread(const FRenderThread&) = delete;
	FRenderThread& operator=(const FRenderThread&) = delete;

	void Startup();
	void Enqueue(std::function<void()>&& Command, std::string_view CommandName = "Render Command");
	void EnqueueAndWait(std::function<void()>&& Command);
	void Flush();
	void Shutdown();

private:
	struct FQueuedCommand
	{
		std::function<void()> Execute;
		std::string Name;
	};

	void Run();

	std::queue<FQueuedCommand> Commands;
	
	std::mutex QueueMutex;
	std::condition_variable QueueCondition;
	
	std::thread Thread;
	std::thread::id RenderThreadId;
	
	bool bStopRequested = false;
};
