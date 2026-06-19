#include "Launch.h"

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
	FEngineLoop EngineLoop(&CreateEngine);

	EngineLoop.Init(Instance, ShowCmd);
	const int32 Result = EngineLoop.Run();
	EngineLoop.Shutdown();

	return Result;
}
