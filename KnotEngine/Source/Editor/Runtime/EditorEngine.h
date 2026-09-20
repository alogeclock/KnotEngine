#pragma once

#include "Render/RenderBackend.h"
#include "Render/Renderer.h"
#include "Runtime/Engine.h"
#include "Asset/AssetImportManager.h"
#include "Asset/AssetRegistry.h"
#include "Input/InputRouter.h"
#include "Editor/ImGuiSystem.h"

#include <filesystem>

class FEditorViewportClient;

UCLASS()
class UEditorEngine : public UEngine
{
	GENERATED_CLASS(UEditorEngine, UEngine)

public:
	explicit UEditorEngine(FWindowsApplication& Application);
	~UEditorEngine() override = default;

	// Editor 모듈 전체의 리플렉션 등록/해제 진입점. 구현은 Reflection.gen.cpp에서 생성한다.
	static void RegisterTypes(FReflectionRegistry& Registry);
	static void ResetTypes();

	void Startup(FWindowsApplication& Application) override;
	void ProcessInput(const FInputSnapshot& InputSnapshot) override;
	void OnWindowResized(FWindowSize Size) override;
	void Tick(float DeltaTime) override;
	void Shutdown() override;

	void RegisterViewportClient(FEditorViewportClient& ViewportClient);
	void UnregisterViewportClient(FEditorViewportClient& ViewportClient);

	void NewLevel();
	bool LoadLevel(const std::filesystem::path& FilePath);
	bool SaveLevel(const std::filesystem::path& FilePath = {});

	UWorld* GetWorld() const override { return FindWorld(EditorContextId); }
	FEditorSelection& GetEditorSelection() { return EditorSelection; }
	const FEditorSelection& GetEditorSelection() const { return EditorSelection; }

private:
	void ProcessAssetImports();
	void Render();

	std::unique_ptr<IRenderBackend> RenderBackend;
	URenderer Renderer;

	TArray<FEditorViewportClient*> AllViewportClients;

	FAssetImportManager AssetImportManager;
	FInputRouter InputRouter;
	FEditorSelection EditorSelection;
	FImGuiSystem ImGuiSystem;

	std::filesystem::path CurrentLevelPath;
	uint64 EditorContextId = 0;
};
