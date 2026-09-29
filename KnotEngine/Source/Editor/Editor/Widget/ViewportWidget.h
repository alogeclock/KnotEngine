#pragma once

#include "Core/Math/Vector2.h"
#include "Viewport/Viewport.h"

class FEditorViewportClient;
class FInputRouter;
class FRenderSystem;

// Editor Viewport의 offscreen surface를 ImGui에 표시하고 해당 이미지 영역을 입력 대상으로 등록한다.
class FViewportWidget final
{
public:
	FViewportWidget(FRenderSystem& InRenderSystem, FInputRouter& InInputRouter);

	bool Draw(FEditorViewportClient& ViewportClient, bool bFocused = true, bool bResize = true);
	void Resize(const FVector2& Size);
	void Release(FEditorViewportClient& ViewportClient);

	FViewport& GetViewport() { return Viewport; }

private:
	FInputRouter& InputRouter;
	FViewport Viewport;
};
