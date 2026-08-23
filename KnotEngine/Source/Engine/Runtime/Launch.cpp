#include "Launch.h"

#include "Core/Debug.h"
#include "Runtime/EngineLoop.h"

int Launch(HINSTANCE Instance, int ShowCmd)
{
	FDebug::Startup();
	FEngineLoop EngineLoop;

	EngineLoop.Startup(Instance, ShowCmd);
	const int32 Result = EngineLoop.Run();
	EngineLoop.Shutdown();

	FDebug::Shutdown();
	return Result;
}
