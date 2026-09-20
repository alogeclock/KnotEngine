#pragma once

#include "Asset/Texture/Texture.h"

class FAssetBinaryLoader;

// Texture2D Mip 하나의 연속 Pixel/Block 데이터와 행 간격을 소유한다.
struct ENGINE_API FTextureMipData
{
	TArray<uint8> Bytes;
	uint32 RowPitch = 0;
};

// 2차원 Texture의 CPU Mip 데이터를 소유한다. GPU Resource는 Renderer가 별도로 관리한다.
UCLASS()
class ENGINE_API UTexture2D final : public UTexture
{
	GENERATED_CLASS(UTexture2D, UTexture)

public:
	EAssetType GetAssetType() const override { return EAssetType::Texture2D; }
	const TArray<FTextureMipData>& GetMips() const { return Mips; }

private:
	friend class FAssetBinaryLoader;
	bool Initialize(
		const FAssetId& InAssetId,
		FString InAssetPath,
		uint32 InWidth,
		uint32 InHeight,
		ETextureFormat InFormat,
		ETextureColorSpace InColorSpace,
		TArray<FTextureMipData>&& InMips);

	TArray<FTextureMipData> Mips;
};
