#include "Launch.h"

#include "Core/Debug.h"
#include "EditorEngine.h"
#include "Render/RenderSystem.h"
#include "Runtime/EngineLoop.h"

// 런처 진입점, 엔진 루프의 생성/실행/종료를 래핑.
int Launch(HINSTANCE Instance, int ShowCmd)
{
	FDebug::Startup();
	FEngineLoop EngineLoop;
	EngineLoop.Startup(Instance, ShowCmd);

	UEditorEngine::RegisterTypes(EngineLoop.GetReflectionRegistry());

	FRenderSystem RenderSystem;
	RenderSystem.Startup(EngineLoop.GetApplication().GetWindow().GetHwnd());

	UEditorEngine* EditorEngine = GUObjectManager.Create<UEditorEngine>(EngineLoop.GetApplication(), RenderSystem);
	GEngine = EditorEngine;

	EditorEngine->Startup(EngineLoop.GetApplication());
	EngineLoop.GetApplication().Show();

	const int32 Result = EngineLoop.Run(*EditorEngine);

	EditorEngine->Shutdown();
	GUObjectManager.Destroy(EditorEngine);
	GEngine = nullptr;
	
	RenderSystem.Shutdown();

	UEditorEngine::ResetTypes();

	EngineLoop.Shutdown();
	FDebug::Shutdown();

	return Result;
}
