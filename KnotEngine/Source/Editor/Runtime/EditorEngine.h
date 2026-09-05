#pragma once

#include "Render/D3D11/D3D11RenderContext.h"
#include "Render/D3D11/D3D11RenderDevice.h"
#include "Render/ImGui/D3D11ImGuiBackend.h"
#include "Render/Renderer.h"
#include "Runtime/Engine.h"
#include "Input/InputRouter.h"
#include "UI/EditorUISystem.h"

class UCubeComponent;

UCLASS()
class UEditorEngine : public UEngine
{
	GENERATED_CLASS(UEditorEngine, UEngine)

public:
	UEditorEngine();
	~UEditorEngine() override = default;

	// Editor 모듈 전체의 리플렉션 등록/해제 진입점. 구현은 Reflection.gen.cpp에서 생성한다.
	static void RegisterTypes(FReflectionRegistry& Registry);
	static void ResetTypes();

	void Startup(FWindowsWindow InWindow) override;
	void ProcessInput(const FInputSnapshot& InputSnapshot) override;
	void Tick(float DeltaTime) override;
	void Shutdown() override;

private:
	FD3D11RenderDevice RenderDevice;
	FD3D11RenderContext RenderContext;
	URenderer Renderer;
	FD3D11ImGuiBackend ImGuiRenderBackend;

	FInputRouter InputRouter;
	FEditorUISystem EditorUISystem;

	UPROPERTY(NoEdit, Transient) TObjectPtr<UCubeComponent> Cube;
};
