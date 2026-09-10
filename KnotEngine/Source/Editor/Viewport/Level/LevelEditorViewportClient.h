#pragma once

#include "Viewport/EditorViewportClient.h"

class URenderer;

class FLevelEditorViewportClient : public FEditorViewportClient
{
public:
	explicit FLevelEditorViewportClient(FViewport& InViewport);

protected:
	void DrawViewport(URenderer& Renderer) override;
};
