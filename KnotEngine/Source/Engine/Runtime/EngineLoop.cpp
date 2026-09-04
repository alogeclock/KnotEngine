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

void FEngineLoop::Startup(HINSTANCE Instance, int32 ShowCmd)
{
	FName::Startup();
	ReflectionRegistry.Startup();

	Application.Startup(Instance, ShowCmd);

	check(!GEngine);

#if WITH_EDITOR
	GEngine = GUObjectManager.Create<UEditorEngine>();
#else
	GEngine = GUObjectManager.Create<UGameEngine>();
#endif

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

	ReflectionRegistry.Shutdown();
	FName::Shutdown();
}
