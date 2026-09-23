#include "EngineLoop.h"

#include "Core/Assert.h"
#include "Core/Name.h"
#include "Core/Profiling/CPUProfiler.h"
#include "Object/Object.h"
#include "Runtime/Engine.h"

void FEngineLoop::Startup(HINSTANCE Instance, int32 ShowCmd)
{
	FName::Startup();
	GUObjectManager.Startup();
	ReflectionRegistry.Startup();

	Application.Startup(Instance, ShowCmd);
}

int32 FEngineLoop::Run(UEngine& Engine)
{
	FrameTimer.Reset();

	while (!Application.IsExitRequested())
	{
		FrameTimer.Tick();

		Application.PumpMessages();
		if (Application.IsExitRequested())
		{
			break;
		}

		if (const std::optional<FWindowSize> Resize = Application.ConsumePendingResize())
		{
			Engine.OnWindowResized(*Resize);
		}

#if KNOT_CPU_PROFILER_ENABLED
		FCPUProfiler::BeginFrame(ECPUProfileThread::Game, FrameTimer.GetDeltaTime());
#endif
		Engine.ProcessInput(Application.GetInputSnapshot());
		Engine.Tick(FrameTimer.GetDeltaTime());
#if KNOT_CPU_PROFILER_ENABLED
		FCPUProfiler::EndFrame();
#endif
	}

	return 0;
}

void FEngineLoop::Shutdown()
{
	Application.Shutdown();

	ReflectionRegistry.Shutdown();
	GUObjectManager.Shutdown();
	FName::Shutdown();
}
