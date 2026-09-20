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
