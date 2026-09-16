#include "Render/Resource/Texture.h"

#include "Render/RHI/RenderDevice.h"

#include <utility>

FTexture::~FTexture()
{
	Release();
}

FTexture::FTexture(FTexture&& Other) noexcept
	: RenderDevice(std::exchange(Other.RenderDevice, nullptr)),
	  Desc(Other.Desc),
	  TextureHandle(std::exchange(Other.TextureHandle, {}))
{
	bInitialized = std::exchange(Other.bInitialized, false);
}

FTexture& FTexture::operator=(FTexture&& Other) noexcept
{
	if (this != &Other)
	{
		Release();
		RenderDevice = std::exchange(Other.RenderDevice, nullptr);
		Desc = Other.Desc;
		TextureHandle = std::exchange(Other.TextureHandle, {});
		bInitialized = std::exchange(Other.bInitialized, false);
	}
	return *this;
}

// Texture Description과 Mip 데이터를 GPU에 업로드하고 생성된 Handle을 소유한다.
bool FTexture::Initialize(IRenderDevice& InRenderDevice, const FTextureDesc& InDesc, std::span<const FTextureSubresourceData> InitialData)
{
	Release();

	FTextureHandle NewHandle = InRenderDevice.CreateTexture(InDesc, InitialData);
	if (!NewHandle.IsValid())
	{
		return false;
	}

	RenderDevice = &InRenderDevice;
	Desc = InDesc;
	TextureHandle = NewHandle;
	bInitialized = true;
	return true;
}

// 소유 중인 GPU Texture를 생성한 Render Device를 통해 해제한다.
void FTexture::OnRelease()
{
	if (RenderDevice)
	{
		RenderDevice->DestroyTexture(TextureHandle);
	}
	RenderDevice = nullptr;
	Desc = {};
}
