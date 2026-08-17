#include "Launch.h"

#include "Core/Debug.h"

#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#else
#include "Runtime/GameEngine.h"
#endif

#include "Runtime/EngineLoop.h"

static void CreateEngine()
{
#if WITH_EDITOR
	GEngine = GUObjectManager.Create<UEditorEngine>();
#else
	GEngine = GUObjectManager.Create<UGameEngine>();
#endif
}

int Launch(HINSTANCE Instance, int ShowCmd)
{
	FDebug::Startup();
	FEngineLoop EngineLoop(&CreateEngine);

	EngineLoop.Startup(Instance, ShowCmd);
	const int32 Result = EngineLoop.Run();
	EngineLoop.Shutdown();

	FDebug::Shutdown();
	return Result;
}
