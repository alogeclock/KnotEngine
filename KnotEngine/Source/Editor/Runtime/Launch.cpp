#include "Launch.h"

#include "Core/Debug.h"
#include "EditorEngine.h"
#include "Runtime/EngineLoop.h"

#include <imgui.h>
#include <imgui_impl_win32.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND WindowHandle, UINT Message, WPARAM WParam, LPARAM LParam);

// 런처 진입점, 엔진 루프의 생성/실행/종료를 래핑.
int Launch(HINSTANCE Instance, int ShowCmd)
{
	FDebug::Startup();
	FEngineLoop EngineLoop;
	EngineLoop.Startup(Instance, ShowCmd);
	UEditorEngine::RegisterTypes(EngineLoop.GetReflectionRegistry());

	UEditorEngine* EditorEngine = GUObjectManager.Create<UEditorEngine>();
	GEngine = EditorEngine;

	EditorEngine->Startup(EngineLoop.GetApplication().GetWindow());
	EngineLoop.GetApplication().SetMessageHandler(ImGui_ImplWin32_WndProcHandler);

	const int32 Result = EngineLoop.Run(*EditorEngine);

	EngineLoop.GetApplication().SetMessageHandler(nullptr);
	EditorEngine->Shutdown();
	GUObjectManager.Destroy(EditorEngine);
	GEngine = nullptr;
	UEditorEngine::ResetTypes();

	EngineLoop.Shutdown();
	FDebug::Shutdown();

	return Result;
}
