#pragma once

#include "Viewport/EditorViewportClient.h"

class URenderer;
class UNode;
struct FEditorSelection;

class FLevelEditorViewportClient : public FEditorViewportClient
{
public:
	FLevelEditorViewportClient(FViewport& InViewport, FEditorSelection& InSelection);

	FSceneView BuildSceneView() override;
	FInputReply OnInputEvent(const FInputEvent& Event) override;

private:
	UNode* Raycast(const FVector2& InputPosition);

	FEditorSelection& Selection;
};
