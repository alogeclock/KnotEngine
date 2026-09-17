#pragma once

#include "Asset/Texture/Texture.h"
#include "Render/Resource/Texture.h"

class FAssetBinaryLoader;

// Texture2D Mip 하나의 연속 Pixel/Block 데이터와 행 간격을 소유한다.
struct ENGINE_API FTextureMipData
{
	TArray<uint8> Bytes;
	uint32 RowPitch = 0;
};

// 2차원 Texture의 CPU Mip 데이터와 대응하는 GPU FTexture 리소스를 소유한다.
UCLASS()
class ENGINE_API UTexture2D final : public UTexture
{
	GENERATED_CLASS(UTexture2D, UTexture)

public:
	bool InitResources(IRenderDevice& RenderDevice) override;
	void ReleaseResources() override;

	FTexture* GetResource() override { return &TextureResource; }
	const FTexture* GetResource() const override { return &TextureResource; }

	const TArray<FTextureMipData>& GetMips() const { return Mips; }

private:
	friend class FAssetBinaryLoader;
	bool Initialize(
		FString InAssetPath,
		uint32 InWidth,
		uint32 InHeight,
		ETextureFormat InFormat,
		ETextureColorSpace InColorSpace,
		TArray<FTextureMipData>&& InMips);

	TArray<FTextureMipData> Mips;
	FTexture TextureResource;
};
