#include "Launch.h"

#include <cassert>

#if WITH_EDITOR
	#include "Editor/EditorEngine.h"
#else
	#include "Runtime/GameEngine.h"
#endif

#include "Runtime/EngineLoop.h"

static UEngine* CreateEngine()
{
#if WITH_EDITOR
	GEngine = GUObjectManager.Create<UEditorEngine>();
#else
	GEngine = GUObjectManager.Create<UGameEngine>();
#endif

	assert(GEngine);
	return GEngine;
}

int Launch(HINSTANCE Instance, int ShowCmd)
{
	FEngineLoop EngineLoop(&CreateEngine);

	EngineLoop.Init(Instance, ShowCmd);
	const int Result = EngineLoop.Run();
	EngineLoop.Shutdown();

	return Result;
}
