#pragma once

#include <Windows.h>

#include "Object/Object.h"

enum class EEngineType
{
	Editor,
	Game,
};

class UEngine : public UObject
{
public:
    virtual ~UEngine() = default;

	virtual bool Init(HINSTANCE Instance, int ShowCmd) { return true; }
	virtual void Tick(float DeltaTime) {}
	virtual void Shutdown() {}
};

extern UEngine* GEngine;
