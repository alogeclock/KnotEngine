#pragma once

#include "Viewport/EditorViewportClient.h"

class UWorld;

// Asset Preview World를 바라보며 공통 에디터 카메라 입력과 ViewFamily 생성을 제공한다.
class FAssetEditorViewportClient : public FEditorViewportClient
{
public:
	explicit FAssetEditorViewportClient(FViewport& InViewport);

	void SetWorld(UWorld* InWorld) { World = InWorld; }
	UWorld* GetWorld() const override { return World; }
	FSceneView BuildSceneView() override;

	int32& GetForcedLODIndex() { return ForcedLODIndex; }

private:
	UWorld* World = nullptr;
	int32 ForcedLODIndex = -1;
};
