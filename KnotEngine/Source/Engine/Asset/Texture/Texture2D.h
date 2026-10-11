#pragma once

#include "Asset/Texture/Texture.h"
#include <memory>

class FAssetBinaryLoader;

// 불변 Texture 데이터의 소유 참조를 유지한다. GPU Resource는 Renderer가 별도로 관리한다.
UCLASS()
class ENGINE_API UTexture2D final : public UTexture
{
	GENERATED_CLASS(UTexture2D, UTexture)

public:
	EAssetType GetAssetType() const override { return EAssetType::Texture2D; }
	const std::shared_ptr<const FTexture>& GetTextureData() const { return TextureData; }

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

	std::shared_ptr<const FTexture> TextureData;
};
