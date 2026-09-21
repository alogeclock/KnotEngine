#pragma once

#include "Asset/Asset/AssetId.h"
#include "Viewport/Asset/AssetEditorViewportClient.h"
#include "Viewport/Viewport.h"

class FInputRouter;
class FRenderSystem;
class FViewportToolbar;
class UAsset;
class UEditorEngine;
class UWorld;

// 특정 Asset 하나를 편집하는 동적 Document Panel과 독립 Preview World의 수명을 관리한다.
class FAssetEditor
{
public:
	FAssetEditor(
		UEditorEngine& InEditorEngine,
		FRenderSystem& InRenderSystem,
		FInputRouter& InInputRouter,
		FViewportToolbar& InViewportToolbar,
		UAsset& InAsset);
	virtual ~FAssetEditor();

	FAssetEditor(const FAssetEditor&) = delete;
	FAssetEditor& operator=(const FAssetEditor&) = delete;

	void Startup();
	void Draw(float DeltaTime);
	void Release();

	const FAssetId& GetAssetId() const { return AssetId; }
	FAssetEditorViewportClient& GetViewportClient() { return ViewportClient; }
	bool IsOpen() const { return bOpen; }
	void RequestFocus() { bFocusRequested = true; }

protected:
	virtual const char* GetEditorTypeName() const = 0;

	virtual void CreatePreviewContent(UWorld& PreviewWorld) = 0;
	virtual void DestroyPreviewContent() {}

	virtual void DrawAssetToolbar() {}
	virtual void DrawAssetDetails() = 0;

	UAsset& GetAsset() const { return Asset; }
	UWorld* GetPreviewWorld() const;
	FAssetEditorViewportClient& GetAssetViewportClient() { return ViewportClient; }

private:
	void DrawToolbar();
	void DrawViewport();

	UEditorEngine& EditorEngine;
	FRenderSystem& RenderSystem;
	FInputRouter& InputRouter;
	FViewportToolbar& ViewportToolbar;
	UAsset& Asset;

	FAssetId AssetId;
	FString WindowName;
	uint64 PreviewWorldContextId = 0;

	FViewport Viewport;
	FAssetEditorViewportClient ViewportClient;

	bool bOpen = true;
	bool bFocusRequested = false;
	bool bStarted = false;
};
