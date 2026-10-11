#include "Asset/Texture/Texture2D.h"

#include <limits>

// Texture 메타데이터와 Mip을 검증하고 하나의 불변 버전으로 게시한다.
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

	FTexture Texture;
	Texture.Desc.Width = InWidth;
	Texture.Desc.Height = InHeight;
	Texture.Desc.MipCount = static_cast<uint32>(InMips.size());
	Texture.Desc.Format = InFormat;
	Texture.Desc.Usage = ETextureUsage::ShaderResource;
	Texture.Desc.bSRGB = InColorSpace == ETextureColorSpace::SRGB;
	Texture.Mips = std::move(InMips);
	TextureData = std::make_shared<const FTexture>(std::move(Texture));
	return true;
}
