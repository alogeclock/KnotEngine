#pragma once

#include "Render/RenderSystem.h"

class FRenderSystem;

class FViewport
{
public:
	explicit FViewport(FRenderSystem& InRenderSystem);
	~FViewport();

	FViewport(const FViewport&) = delete;
	FViewport& operator=(const FViewport&) = delete;

	void Resize(uint32 Width, uint32 Height);
	void Release();

	FTextureHandle GetSceneColorTarget() const { return Targets.RenderTarget.SceneColor; }
	FTextureHandle GetDisplayColorTarget() const { return Targets.RenderTarget.DisplayColor; }
	FTextureHandle GetSelectionDepthTarget() const { return Targets.RenderTarget.SelectionDepth; }
	FTextureHandle GetDepthTarget() const { return Targets.RenderTarget.Depth; }
	ImTextureID GetDisplayTextureId() const { return Targets.DisplayTextureId; }
	FRenderViewport GetRenderViewport() const;

	uint32 GetWidth() const { return Targets.RenderTarget.Width; }
	uint32 GetHeight() const { return Targets.RenderTarget.Height; }

	bool IsValid() const
	{
		return Targets.RenderTarget.SceneColor.IsValid() && Targets.RenderTarget.DisplayColor.IsValid() &&
			Targets.RenderTarget.SelectionDepth.IsValid() && Targets.RenderTarget.Depth.IsValid();
	}

private:
	FRenderSystem& RenderSystem;

	FViewportRenderTargets Targets;
};
