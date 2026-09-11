#include "Launch.h"

#include "Core/Debug.h"
#include "EditorEngine.h"
#include "Runtime/EngineLoop.h"


// 런처 진입점, 엔진 루프의 생성/실행/종료를 래핑.
int Launch(HINSTANCE Instance, int ShowCmd)
{
	FDebug::Startup();
	FEngineLoop EngineLoop;
	EngineLoop.Startup(Instance, ShowCmd);
	UEditorEngine::RegisterTypes(EngineLoop.GetReflectionRegistry());

	UEditorEngine* EditorEngine = GUObjectManager.Create<UEditorEngine>(EngineLoop.GetApplication());
	GEngine = EditorEngine;

	EditorEngine->Startup(EngineLoop.GetApplication());

	const int32 Result = EngineLoop.Run(*EditorEngine);

	EditorEngine->Shutdown();
	GUObjectManager.Destroy(EditorEngine);
	GEngine = nullptr;
	UEditorEngine::ResetTypes();

	EngineLoop.Shutdown();
	FDebug::Shutdown();

	return Result;
}
