#pragma once

#include "Runtime/WindowsApplication.h"

class UEngine;
struct FEngineInitParams;

class FEngineLoop
{
public:
    using FCreateEngineFn = std::unique_ptr<UEngine> (*)(const FEngineInitParams&);
	explicit FEngineLoop(FCreateEngineFn InFactory);

	bool Init(const FEngineInitParams& Params);
    int Run();
    void Shutdown();
    
private:
	std::unique_ptr<UEngine> Engine;
    FWindowsApplication Application;
};
