#include "LevelEditorViewportClient.h"

#include "Core/Math/Matrix.h"
#include "Render/Renderer.h"
#include "Runtime/EditorEngine.h"
#include "Viewport/Viewport.h"
#include "World/World.h"

#include <variant>

FLevelEditorViewportClient::FLevelEditorViewportClient(UEditorEngine& InEditorEngine, FViewport& InViewport)
	: FEditorViewportClient(InViewport), EditorEngine(InEditorEngine)
{
}

FInputReply FLevelEditorViewportClient::OnInputEvent(const FInputEvent& Event)
{
	const FPointerInputEvent* PointerEvent = std::get_if<FPointerInputEvent>(&Event);
	if (!PointerEvent)
	{
		return FInputReply::Unhandled();
	}

	if (PointerEvent->Type == EPointerInputEventType::ButtonDown)
	{
		return FInputReply::Handled().SetKeyboardFocus().CaptureMouse();
	}
	if (PointerEvent->Type == EPointerInputEventType::ButtonUp)
	{
		return FInputReply::Handled().ReleaseMouse();
	}
	return FInputReply::Handled();
}

void FLevelEditorViewportClient::DrawViewport(URenderer& Renderer)
{
	UWorld* World = EditorEngine.GetEditorWorld();
	if (!World)
	{
		return;
	}

	FViewport& Viewport = GetViewport();
	const FRenderViewport ViewportInfo = Viewport.GetRenderViewport();
	const float AspectRatio = ViewportInfo.Width / ViewportInfo.Height;
	const FMatrix View = FMatrix::MakeLookAt(FVector(-5.0f, 0.0f, 0.0f), FVector::ZeroVector, FVector::UpVector);
	const FMatrix Projection = FMatrix::MakePerspectiveFov(KMath::ToRadian(60.0f), AspectRatio, 0.1f, 100.0f);

	Renderer.BeginRenderTarget(Viewport.GetColorTarget(), Viewport.GetDepthTarget(), ViewportInfo);
	World->Render(Renderer, View * Projection);
	Renderer.EndRenderTarget();
}
