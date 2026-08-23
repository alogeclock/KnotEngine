#pragma once

#include "Runtime/FrameTimer.h"
#include "Runtime/WindowsApplication.h"

class UEngine;

// 빌드 구성(WITH_EDITOR)에 따라 엔진을 생성하고, 엔진 실행/종료 및 엔진 루프를 관리하는 객체
class FEngineLoop
{
public:
	void Startup(HINSTANCE Instance, int32 ShowCmd);
	int32 Run();
	void Shutdown();

private:
	FWindowsApplication Application; // Windows 전용으로 사용, 추후 플랫폼 확장 시 FGenericApplication으로 대응
	FFrameTimer FrameTimer;
};
