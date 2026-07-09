#pragma once

#include "Runtime/Engine.h"
#include "Render/Renderer.h"
#include "UI/EditorUI.h"
#include "UI/ImGuiSystem.h"

// [TODO] 큐브 삭제
#include "Components/CubeComponent.h"

class UEditorEngine : public UEngine
{
public:
    virtual ~UEditorEngine() = default;

	void Startup(FWindowsWindow InWindow) override;
	void Tick(float DeltaTime) override;
	void Shutdown() override;

private:
	URenderer Renderer;
	FImGuiSystem ImGuiSystem;
	FEditorUI EditorUI;

	UCubeComponent* Cube = nullptr;
};
