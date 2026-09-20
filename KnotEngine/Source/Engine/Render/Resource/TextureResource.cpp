#include "Render/Resource/TextureResource.h"

#include "Render/RHI/RenderDevice.h"

#include <utility>

FTextureResource::~FTextureResource()
{
	Release();
}

FTextureResource::FTextureResource(FTextureResource&& Other) noexcept
    : RenderDevice(std::exchange(Other.RenderDevice, nullptr)),
      Desc(Other.Desc),
      TextureHandle(std::exchange(Other.TextureHandle, {})),
      SourceRevision(std::exchange(Other.SourceRevision, 0))
{
	bInitialized = std::exchange(Other.bInitialized, false);
}

FTextureResource& FTextureResource::operator=(FTextureResource&& Other) noexcept
{
	if (this != &Other)
	{
		Release();
		RenderDevice = std::exchange(Other.RenderDevice, nullptr);
		Desc = Other.Desc;
		TextureHandle = std::exchange(Other.TextureHandle, {});
		SourceRevision = std::exchange(Other.SourceRevision, 0);
		bInitialized = std::exchange(Other.bInitialized, false);
	}
	return *this;
}

// Texture Description과 Mip 데이터를 GPU에 업로드하고 원본 Asset Revision을 함께 기록한다.
bool FTextureResource::Initialize(IRenderDevice& InRenderDevice, const FTextureDesc& InDesc, std::span<const FTextureSubresourceData> InitialData, uint64 InSourceRevision)
{
	Release();
	if (InSourceRevision == 0)
	{
		return false;
	}

	FTextureHandle NewHandle = InRenderDevice.CreateTexture(InDesc, InitialData);
	if (!NewHandle.IsValid())
	{
		return false;
	}

	RenderDevice = &InRenderDevice;
	Desc = InDesc;
	TextureHandle = NewHandle;
	SourceRevision = InSourceRevision;
	bInitialized = true;
	return true;
}

// 소유 중인 GPU Texture를 생성한 Render Device를 통해 해제한다.
void FTextureResource::OnRelease()
{
	if (RenderDevice)
	{
		RenderDevice->DestroyTexture(TextureHandle);
	}
	RenderDevice = nullptr;
	Desc = {};
	SourceRevision = 0;
}
