#include "EngineLoop.h"

#include "Core/Assert.h"
#include "Core/Name.h"
#include "Runtime/Engine.h"

void FEngineLoop::Startup(HINSTANCE Instance, int32 ShowCmd)
{
	FName::Startup();
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

		Engine.ProcessInput(Application.GetInputSnapshot());
		Engine.Tick(FrameTimer.GetDeltaTime());
	}

	return 0;
}

void FEngineLoop::Shutdown()
{
	Application.Shutdown();

	ReflectionRegistry.Shutdown();
	FName::Shutdown();
}
