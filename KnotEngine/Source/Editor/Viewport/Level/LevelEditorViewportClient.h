#pragma once

#include "Viewport/EditorViewportClient.h"

class URenderer;
class UNode;
struct FEditorSelection;

class FLevelEditorViewportClient : public FEditorViewportClient
{
public:
	FLevelEditorViewportClient(FViewport& InViewport, FEditorSelection& InSelection);

	FInputReply OnInputEvent(const FInputEvent& Event) override;

private:
	UNode* Raycast(const FVector2& InputPosition);

	FEditorSelection& Selection;
};
