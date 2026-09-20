#include "Viewport.h"

#include "Render/RenderSystem.h"

FViewport::FViewport(FRenderSystem& InRenderSystem)
	: RenderSystem(InRenderSystem)
{
}

FViewport::~FViewport()
{
	Release();
}

void FViewport::Resize(uint32 InWidth, uint32 InHeight)
{
	if (InWidth == 0 || InHeight == 0 || (GetWidth() == InWidth && GetHeight() == InHeight))
	{
		return;
	}
	Targets = RenderSystem.ResizeViewportTargets(std::move(Targets), InWidth, InHeight);
}

void FViewport::Release()
{
	RenderSystem.ReleaseViewportTargets(Targets);
}

FRenderViewport FViewport::GetRenderViewport() const
{
	return { 0.0f, 0.0f, static_cast<float>(GetWidth()), static_cast<float>(GetHeight()), 0.0f, 1.0f };
}
