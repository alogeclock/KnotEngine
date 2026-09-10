#include "LevelEditorViewportClient.h"

#include "Core/Math/Matrix.h"
#include "Render/Renderer.h"
#include "Viewport/Viewport.h"
#include "World/World.h"

FLevelEditorViewportClient::FLevelEditorViewportClient(FViewport& InViewport)
	: FEditorViewportClient(InViewport)
{
}

void FLevelEditorViewportClient::DrawViewport(URenderer& Renderer)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FViewport& Viewport = GetViewport();
	const FRenderViewport ViewportInfo = Viewport.GetRenderViewport();

	Renderer.BeginRenderTarget(Viewport.GetColorTarget(), Viewport.GetDepthTarget(), ViewportInfo);
	World->Render(Renderer, GetViewProjectionMatrix());
	Renderer.EndRenderTarget();
}
