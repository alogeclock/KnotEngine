#include "Viewport.h"

#include "Render/RHI/RenderDevice.h"

FViewport::FViewport(IRenderDevice& InRenderDevice)
	: RenderDevice(InRenderDevice)
{
}

FViewport::~FViewport()
{
	Release();
}

void FViewport::Resize(uint32 InWidth, uint32 InHeight)
{
	if (InWidth == 0 || InHeight == 0 || (Width == InWidth && Height == InHeight))
	{
		return;
	}

	Release();
	ColorTarget = RenderDevice.CreateTexture({ InWidth, InHeight, ETextureFormat::BGRA8UNorm, ETextureUsage::RenderTarget | ETextureUsage::ShaderResource });
	DepthTarget = RenderDevice.CreateTexture({ InWidth, InHeight, ETextureFormat::D24UNormS8UInt, ETextureUsage::DepthStencil });
	Width = InWidth;
	Height = InHeight;
}

void FViewport::Release()
{
	RenderDevice.DestroyTexture(DepthTarget);
	RenderDevice.DestroyTexture(ColorTarget);
	Width = 0;
	Height = 0;
}

FRenderViewport FViewport::GetRenderViewport() const
{
	return { 0.0f, 0.0f, static_cast<float>(Width), static_cast<float>(Height), 0.0f, 1.0f };
}
