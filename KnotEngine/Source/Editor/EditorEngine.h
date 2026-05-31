#pragma once

#include "Runtime/Engine.h"
#include "Render/Renderer.h"

class UEditorEngine : public UEngine
{
public:
    virtual ~UEditorEngine() = default;

	void Init(FWindowsWindow InWindow) override;
	void Tick(float DeltaTime) override;
	void Shutdown() override;

private:
	URenderer Renderer;
};