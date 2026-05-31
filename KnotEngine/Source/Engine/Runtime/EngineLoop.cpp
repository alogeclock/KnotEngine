#include "EngineLoop.h"

#if WITH_EDITOR
	#include "Editor/EditorEngine.h"
#else
	#include "Runtime/GameEngine.h"
#endif

FEngineLoop::FEngineLoop(FCreateEngineFn InFactory) : CreateEngine(InFactory)
{
	assert(CreateEngine);
}

void FEngineLoop::Init(HINSTANCE Instance, int ShowCmd)
{
	Application.Init(Instance, ShowCmd);
	GEngine = CreateEngine();
	GEngine->Init(Instance, ShowCmd);
}

int FEngineLoop::Run()
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
