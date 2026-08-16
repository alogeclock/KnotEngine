#pragma once

#include "Object/Object.h"
#include "Runtime/WindowsWindow.h"

class UEngine : public UObject
{
public:
	virtual ~UEngine() = default;

	virtual void Startup(FWindowsWindow InWindow) {}
	virtual void Tick(float DeltaTime) {}
	virtual void Shutdown() {}
};

extern UEngine* GEngine;
