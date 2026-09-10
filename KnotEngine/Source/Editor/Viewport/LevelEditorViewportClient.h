#pragma once

#include "EditorViewportClient.h"
#include "Input/InputRouter.h"

class UEditorEngine;
class URenderer;

class FLevelEditorViewportClient : public FEditorViewportClient, public IInputTarget
{
public:
	FLevelEditorViewportClient(UEditorEngine& InEditorEngine, FViewport& InViewport);

	FInputReply OnInputEvent(const FInputEvent& Event) override;

protected:
	void DrawViewport(URenderer& Renderer) override;

private:
	UEditorEngine& EditorEngine;
};
