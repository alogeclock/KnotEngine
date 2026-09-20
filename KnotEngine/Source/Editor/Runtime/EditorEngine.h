#pragma once

#include "Runtime/Engine.h"
#include "Asset/AssetImportManager.h"
#include "Asset/AssetRegistry.h"
#include "Input/InputRouter.h"
#include "Editor/ImGuiSystem.h"
#include "Editor/Settings/EditorSettings.h"

#include <filesystem>

class FEditorViewportClient;
class FRenderSystem;

UCLASS()
class UEditorEngine : public UEngine
{
	GENERATED_CLASS(UEditorEngine, UEngine)

public:
	UEditorEngine(FWindowsApplication& Application, FRenderSystem& InRenderSystem);
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

	FRenderSystem& RenderSystem;

	TArray<FEditorViewportClient*> AllViewportClients;

	FAssetImportManager AssetImportManager;
	FInputRouter InputRouter;
	FEditorSelection EditorSelection;
	FEditorSettings EditorSettings;
	FImGuiSystem ImGuiSystem;

	std::filesystem::path CurrentLevelPath;
	uint64 EditorContextId = 0;
};
