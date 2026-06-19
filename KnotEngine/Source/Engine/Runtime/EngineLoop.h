#pragma once

#include "Runtime/WindowsApplication.h"

class UEngine;

// 엔진 생성/실행/종료 및 엔진 루프를 관리하는 객체, 구체 엔진 타입은 모름
class FEngineLoop
{
public:
    using FCreateEngineFn = void(*)();
	explicit FEngineLoop(FCreateEngineFn InFactory);

	void Init(HINSTANCE Instance, int32 ShowCmd);
    int32 Run();
    void Shutdown();

private:
	FCreateEngineFn CreateEngine = nullptr;
    FWindowsApplication Application; // Windows 전용으로 사용, 추후 플랫폼 확장 시 FGenericApplication으로 대응
};
