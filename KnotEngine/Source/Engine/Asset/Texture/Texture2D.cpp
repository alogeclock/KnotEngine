#include "Asset/Texture/Texture2D.h"

#include <limits>

// Texture 메타데이터와 각 Mip의 CPU Payload를 검증하고 Asset에 저장한다.
bool UTexture2D::Initialize(
	const FAssetId& InAssetId,
	FString InAssetPath,
	uint32 InWidth,
	uint32 InHeight,
	ETextureFormat InFormat,
	ETextureColorSpace InColorSpace,
	TArray<FTextureMipData>&& InMips)
{
	if (InMips.empty() || InMips.size() > (std::numeric_limits<uint32>::max)())
	{
		return false;
	}
	for (const FTextureMipData& Mip : InMips)
	{
		if (Mip.Bytes.empty() || Mip.RowPitch == 0 || Mip.Bytes.size() > (std::numeric_limits<uint32>::max)())
		{
			return false;
		}
	}
	if (!Super::Initialize(InAssetId, std::move(InAssetPath), InWidth, InHeight, static_cast<uint32>(InMips.size()), InFormat, InColorSpace))
	{
		return false;
	}

	Mips = std::move(InMips);
	return true;
}

// CPU에 보관된 모든 Mip을 Subresource View로 구성해 GPU Texture를 생성한다.
bool UTexture2D::InitResources(IRenderDevice& RenderDevice)
{
	if (TextureResource.IsValid())
	{
		return true;
	}

	TArray<FTextureSubresourceData> Subresources;
	Subresources.reserve(Mips.size());
	for (const FTextureMipData& Mip : Mips)
	{
		Subresources.push_back({ Mip.Bytes, Mip.RowPitch, static_cast<uint32>(Mip.Bytes.size()) });
	}

	FTextureDesc Desc;
	Desc.Width = GetWidth();
	Desc.Height = GetHeight();
	Desc.MipCount = GetMipCount();
	Desc.Format = GetFormat();
	Desc.Usage = ETextureUsage::ShaderResource;
	Desc.bSRGB = IsSRGB();
	return TextureResource.Initialize(RenderDevice, Desc, Subresources);
}

// CPU Mip 데이터는 유지하고 GPU Texture 리소스만 해제한다.
void UTexture2D::ReleaseResources()
{
	TextureResource.Release();
}
