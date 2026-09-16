#include "Asset/Texture/Texture.h"

// Texture Asset의 논리 경로와 모든 Mip에 공통인 이미지 속성을 검증해 저장한다.
bool UTexture::Initialize(FString InAssetPath, uint32 InWidth, uint32 InHeight, uint32 InMipCount, ETextureFormat InFormat, bool bInSRGB)
{
	if (InAssetPath.empty() || InWidth == 0 || InHeight == 0 || InMipCount == 0 || InFormat == ETextureFormat::D24UNormS8UInt)
	{
		return false;
	}

	AssetPath = std::move(InAssetPath);
	Width = InWidth;
	Height = InHeight;
	MipCount = InMipCount;
	Format = InFormat;
	bSRGB = bInSRGB;
	return true;
}
