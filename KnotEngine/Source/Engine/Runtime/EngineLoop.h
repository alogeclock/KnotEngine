#pragma once

#include "EngineAPI.h"

#include "Object/Reflection/ReflectionRegistry.h"
#include "Platform/WindowsApplication.h"
#include "Runtime/FrameTimer.h"

class UEngine;

// 공통 실행 수명과 루프. 구체 엔진 객체는 실행 프로그램이 생성하고 소유한다.
class ENGINE_API FEngineLoop
{
public:
	void Startup(HINSTANCE Instance, int32 ShowCmd);
	int32 Run(UEngine& Engine);
	void Shutdown();
	FReflectionRegistry& GetReflectionRegistry() { return ReflectionRegistry; }
	FWindowsApplication& GetApplication() { return Application; }

private:
	FReflectionRegistry ReflectionRegistry;
	FWindowsApplication Application;
	FFrameTimer FrameTimer;
};
