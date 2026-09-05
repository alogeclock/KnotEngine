#pragma once

#include "EngineAPI.h"

#include "Object/Object.h"
#include "Platform/WindowsWindow.h"

class FInputSnapshot;

UCLASS()
class ENGINE_API UEngine : public UObject
{
	GENERATED_CLASS(UEngine, UObject)

public:
	virtual ~UEngine() = default;

	virtual void Startup(FWindowsWindow InWindow) {}
	virtual void ProcessInput(const FInputSnapshot& InputSnapshot) {}
	virtual void Tick(float DeltaTime) {}
	virtual void Shutdown() {}
};

extern ENGINE_API UEngine* GEngine;
