#pragma once

#include "Render/RHI/RenderTypes.h"

class IRenderDevice;

class FViewport
{
public:
	explicit FViewport(IRenderDevice& InRenderDevice);
	~FViewport();

	FViewport(const FViewport&) = delete;
	FViewport& operator=(const FViewport&) = delete;

	void Resize(uint32 Width, uint32 Height);
	void Release();

	FTextureHandle GetColorTarget() const { return ColorTarget; }
	FTextureHandle GetDepthTarget() const { return DepthTarget; }
	FRenderViewport GetRenderViewport() const;
	uint32 GetWidth() const { return Width; }
	uint32 GetHeight() const { return Height; }
	bool IsValid() const { return ColorTarget.IsValid() && DepthTarget.IsValid(); }

private:
	IRenderDevice& RenderDevice;
	FTextureHandle ColorTarget;
	FTextureHandle DepthTarget;
	uint32 Width = 0;
	uint32 Height = 0;
};
