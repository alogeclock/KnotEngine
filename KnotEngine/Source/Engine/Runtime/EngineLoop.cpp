#include "EngineLoop.h"

#include "Core/Assert.h"
#include "Core/Name.h"
#include "Object/Object.h"
#include "Runtime/Engine.h"

#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#else
#include "Runtime/GameEngine.h"
#endif

FEngineLoop::FEngineLoop(FCreateEngineFn InFactory)
	: CreateEngine(InFactory)
{
	check(CreateEngine);
}

void FEngineLoop::Startup(HINSTANCE Instance, int32 ShowCmd)
{
	FName::Startup();

	Application.Startup(Instance, ShowCmd);

	CreateEngine();
	checkf(GEngine, "CreateEngine 팩토리가 GEngine을 설정하지 않았다.");

	GEngine->Startup(Application.GetWindow());
}

int32 FEngineLoop::Run()
{
	check(GEngine);
	FrameTimer.Reset();

	while (!Application.IsExitRequested())
	{
		FrameTimer.Tick();

		Application.PumpMessages();
		if (Application.IsExitRequested())
		{
			break;
		}

		GEngine->ProcessInput(Application.GetInputSnapshot());
		GEngine->Tick(FrameTimer.GetDeltaTime());
	}

	return 0;
}

void FEngineLoop::Shutdown()
{
	check(GEngine);
	GEngine->Shutdown();

	GUObjectManager.Destroy(GEngine);
	GEngine = nullptr;

	Application.Shutdown();

	FName::Shutdown();
}
