#include "Render/RenderThread.h"

#include "Core/Assert.h"

#include <exception>
#include <future>

FRenderThread::~FRenderThread()
{
	Shutdown();
}

void FRenderThread::Startup()
{
	check(!Thread.joinable());
	bStopRequested = false;
	Thread = std::thread(&FRenderThread::Run, this);
}

void FRenderThread::Enqueue(std::function<void()>&& Command, std::string_view CommandName)
{
	check(Thread.joinable());
	{
		std::lock_guard Lock(QueueMutex);
		Commands.push({ std::move(Command), std::string(CommandName) });
	}
	QueueCondition.notify_one();
}

void FRenderThread::EnqueueAndWait(std::function<void()>&& Command)
{
	auto Completion = std::make_shared<std::promise<void>>();
	std::future<void> Future = Completion->get_future();
	Enqueue([Command = std::move(Command), Completion]() mutable
	{
		try
		{
			Command();
			Completion->set_value();
		}
		catch (...)
		{
			Completion->set_exception(std::current_exception());
		}
	}, "EnqueueAndWait");
	Future.get();
}

void FRenderThread::Flush()
{
	if (Thread.joinable())
	{
		EnqueueAndWait([] {});
	}
}

void FRenderThread::Shutdown()
{
	if (!Thread.joinable())
	{
		return;
	}
	Flush();
	{
		std::lock_guard Lock(QueueMutex);
		bStopRequested = true;
	}
	QueueCondition.notify_one();
	Thread.join();
	RenderThreadId = {};
}

void FRenderThread::Run()
{
	RenderThreadId = std::this_thread::get_id();
	while (true)
	{
		FQueuedCommand Command;
		{
			std::unique_lock Lock(QueueMutex);
			QueueCondition.wait(Lock, [this] { return bStopRequested || !Commands.empty(); });
			if (bStopRequested && Commands.empty())
			{
				break;
			}
			Command = std::move(Commands.front());
			Commands.pop();
		}
		try
		{
			Command.Execute();
		}
		catch (const std::exception& Error)
		{
			panicf(false, "Render Command 실행 중 예외가 발생했다. Command={}, Error={}", Command.Name, Error.what());
		}
		catch (...)
		{
			panicf(false, "Render Command 실행 중 알 수 없는 예외가 발생했다. Command={}", Command.Name);
		}
	}
}
