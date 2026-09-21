#include "Viewport/Asset/AssetEditorViewportClient.h"

FAssetEditorViewportClient::FAssetEditorViewportClient(FViewport& InViewport)
	: FEditorViewportClient(InViewport)
{
}

// Asset Preview에서 선택한 LOD를 일반 View 데이터에 반영한다.
FSceneView FAssetEditorViewportClient::BuildSceneView()
{
	FSceneView View = FEditorViewportClient::BuildSceneView();
	View.ForcedLODIndex = ForcedLODIndex;
	return View;
}
