#include "LevelEditorViewportClient.h"

#include "Core/Math/Matrix.h"
#include "Render/Renderer.h"
#include "Viewport/Viewport.h"
#include "World/World.h"

FLevelEditorViewportClient::FLevelEditorViewportClient(FViewport& InViewport)
	: FEditorViewportClient(InViewport)
{
}
