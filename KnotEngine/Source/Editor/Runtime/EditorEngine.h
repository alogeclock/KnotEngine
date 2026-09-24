#pragma once

#include "Runtime/Engine.h"
#include "Asset/AssetImportManager.h"
#include "Asset/AssetRegistry.h"
#include "Input/InputRouter.h"
#include "Editor/ImGuiSystem.h"
#include "Editor/Setting/EditorSettings.h"
#include "Editor/Transaction/TransactionManager.h"

#include <filesystem>
#include <memory>

class FEditorViewportClient;
class FRenderSystem;

UCLASS()
class UEditorEngine : public UEngine
{
	GENERATED_CLASS(UEditorEngine, UEngine)

public:
	UEditorEngine() = default;
	~UEditorEngine() override = default;

	// Editor 모듈 전체의 리플렉션 등록/해제 진입점. 구현은 Reflection.gen.cpp에서 생성한다.
	static void RegisterTypes(FReflectionRegistry& Registry);
	static void ResetTypes();

	void Startup(FWindowsApplication& Application, FRenderSystem& InRenderSystem);
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
	FRenderSystem& GetRenderSystem() { check(RenderSystem); return *RenderSystem; }
	FAssetImportManager& GetAssetImportManager() { return AssetImportManager; }
	FInputRouter& GetInputRouter() { return InputRouter; }
	FEditorSelection& GetEditorSelection() { return EditorSelection; }
	FEditorSettings& GetEditorSettings() { return EditorSettings; }
	FTransactionManager& GetTransactionManager() { return TransactionManager; }

private:
	void ProcessAssetImports();
	void Render();

	FRenderSystem* RenderSystem = nullptr;

	TArray<FEditorViewportClient*> AllViewportClients;

	FAssetImportManager AssetImportManager;
	FInputRouter InputRouter;
	FEditorSelection EditorSelection;
	FEditorSettings EditorSettings;
	FTransactionManager TransactionManager;
	std::unique_ptr<FImGuiSystem> ImGuiSystem;

	std::filesystem::path CurrentLevelPath;
	uint64 EditorContextId = 0;
};
