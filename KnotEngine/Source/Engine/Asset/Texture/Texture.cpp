#include "Asset/Texture/Texture.h"

// Texture Asset의 논리 경로와 모든 Mip에 공통인 이미지 속성을 검증해 저장한다.
bool UTexture::Initialize(
	const FAssetId& InAssetId,
	FString InAssetPath,
	uint32 InWidth,
	uint32 InHeight,
	uint32 InMipCount,
	ETextureFormat InFormat,
	ETextureColorSpace InColorSpace)
{
	if (InWidth == 0 || InHeight == 0 || InMipCount == 0 ||
		InFormat == ETextureFormat::D24UNormS8UInt || InFormat == ETextureFormat::D32Float)
	{
		return false;
	}
	if (!InitializeAsset(InAssetId, std::move(InAssetPath)))
	{
		return false;
	}

	Width = InWidth;
	Height = InHeight;
	MipCount = InMipCount;
	Format = InFormat;
	ColorSpace = InColorSpace;
	Revision = 1;
	return true;
}
