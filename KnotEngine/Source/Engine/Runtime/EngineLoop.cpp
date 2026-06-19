#include "EngineLoop.h"

#if WITH_EDITOR
	#include "Editor/EditorEngine.h"
#else
	#include "Runtime/GameEngine.h"
#endif

FEngineLoop::FEngineLoop(FCreateEngineFn InFactory) : CreateEngine(InFactory)
{
	check(CreateEngine);
}

void FEngineLoop::Init(HINSTANCE Instance, int32 ShowCmd)
{
	Application.Init(Instance, ShowCmd);
	CreateEngine();
	GEngine->Init(Application.GetWindow());
}

int32 FEngineLoop::Run()
{
	while(!Application.IsExitRequested())
	{
		Application.PumpMessages();
		GEngine->Tick(0.0f);
	}

	return 0;
}

void FEngineLoop::Shutdown()
{
	GEngine->Shutdown();

	GUObjectManager.Destroy(GEngine);
	GEngine = nullptr;

	Application.Shutdown();
}
