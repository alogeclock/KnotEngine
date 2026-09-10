#pragma once

#include "ViewportClient.h"

class FViewport;
class URenderer;

// EditorViewportClient는 연결된 Viewport의 출력 가능 여부를 판정하고 구체적인 Scene draw를 파생 클래스에 위임한다.
class FEditorViewportClient : public FViewportClient
{
public:
	explicit FEditorViewportClient(FViewport& InViewport);

	void Draw(URenderer& Renderer);

protected:
	FViewport& GetViewport() const { return Viewport; }
	virtual void DrawViewport(URenderer& Renderer) = 0;

private:
	FViewport& Viewport;
};
