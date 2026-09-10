#include "EditorViewportClient.h"

#include "Viewport/Viewport.h"

FEditorViewportClient::FEditorViewportClient(FViewport& InViewport)
	: Viewport(InViewport)
{
}

void FEditorViewportClient::Draw(URenderer& Renderer)
{
	if (!Viewport.IsValid())
	{
		return;
	}

	DrawViewport(Renderer);
}
