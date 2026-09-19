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
	FTextureDesc ColorDesc;
	ColorDesc.Width = InWidth;
	ColorDesc.Height = InHeight;
	ColorDesc.Format = ETextureFormat::BGRA8UNorm;
	ColorDesc.Usage = ETextureUsage::RenderTarget | ETextureUsage::ShaderResource;
	SceneColorTarget = RenderDevice.CreateTexture(ColorDesc);
	DisplayColorTarget = RenderDevice.CreateTexture(ColorDesc);

	FTextureDesc DepthDesc;
	DepthDesc.Width = InWidth;
	DepthDesc.Height = InHeight;
	DepthDesc.Format = ETextureFormat::D32Float;
	DepthDesc.Usage = ETextureUsage::DepthStencil | ETextureUsage::ShaderResource;
	SelectionDepthTarget = RenderDevice.CreateTexture(DepthDesc);
	DepthDesc.Usage = ETextureUsage::DepthStencil;
	DepthTarget = RenderDevice.CreateTexture(DepthDesc);
	Width = InWidth;
	Height = InHeight;
}

void FViewport::Release()
{
	RenderDevice.DestroyTexture(DepthTarget);
	RenderDevice.DestroyTexture(SelectionDepthTarget);
	RenderDevice.DestroyTexture(DisplayColorTarget);
	RenderDevice.DestroyTexture(SceneColorTarget);
	Width = 0;
	Height = 0;
}

FRenderViewport FViewport::GetRenderViewport() const
{
	return { 0.0f, 0.0f, static_cast<float>(Width), static_cast<float>(Height), 0.0f, 1.0f };
}
