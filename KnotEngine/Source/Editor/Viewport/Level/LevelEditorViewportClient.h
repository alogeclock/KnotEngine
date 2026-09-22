#pragma once

#include "Viewport/EditorViewportClient.h"
#include "TransformGizmo.h"

class FRenderer;
class UNode;
struct FEditorSelection;

class FLevelEditorViewportClient : public FEditorViewportClient
{
public:
	FLevelEditorViewportClient(FViewport& InViewport, FEditorSelection& InSelection);

	void Tick(float DeltaTime) override;
	FInputReply OnInputEvent(const FInputEvent& Event) override;

	void OnKeyboardFocusLost() override;
	void OnMouseCaptureLost() override;

	FSceneView BuildSceneView() override;

private:
	UNode* Raycast(const FVector2& InputPosition);

	FEditorSelection& Selection;
	FTransformGizmo TransformGizmo;
};
