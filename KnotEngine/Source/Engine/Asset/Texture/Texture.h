#pragma once

#include "EngineAPI.h"

#include "Object/Object.h"
#include "Render/RHI/RenderTypes.h"

class FTexture;
class IRenderDevice;

// Texture2D .kasset 전체에 한 번 저장되는 고정 크기 헤더다.
struct FTextureBinaryHeader
{
	inline static constexpr char MagicValue[4] = { 'K', 'T', 'E', 'X' };
	inline static constexpr uint32 CurrentVersion = 1;
	inline static constexpr uint32 SRGBFlag = 1 << 0;

	char Magic[4];
	uint32 Version;
	uint32 Width;
	uint32 Height;
	uint32 MipCount;
	ETextureFormat Format;

	uint8 Flags;
	uint8 Reserved[2];
};
static_assert(sizeof(FTextureBinaryHeader) == 24);

// Texture2D의 각 Mip Payload 앞에 저장되는 데이터 크기와 행 간격이다.
struct FTextureMipBinaryHeader
{
	uint32 DataSize;
	uint32 RowPitch;
};
static_assert(sizeof(FTextureMipBinaryHeader) == 8);

// Texture Asset의 공통 경로와 이미지 메타데이터를 소유하는 UObject 기반 클래스다.
UCLASS()
class ENGINE_API UTexture : public UObject
{
	GENERATED_CLASS(UTexture, UObject)

public:
	const FString& GetAssetPath() const { return AssetPath; }
	uint32 GetWidth() const { return Width; }
	uint32 GetHeight() const { return Height; }
	uint32 GetMipCount() const { return MipCount; }

	ETextureFormat GetFormat() const { return Format; }
	bool IsSRGB() const { return bSRGB; }

	virtual bool InitResources(IRenderDevice& RenderDevice) = 0;
	virtual void ReleaseResources() = 0;

	virtual FTexture* GetResource() = 0;
	virtual const FTexture* GetResource() const = 0;

protected:
	bool Initialize(FString InAssetPath, uint32 InWidth, uint32 InHeight, uint32 InMipCount, ETextureFormat InFormat, bool bInSRGB);

private:
	UPROPERTY(NoEdit) FString AssetPath;

	uint32 Width = 0;
	uint32 Height = 0;
	uint32 MipCount = 0;

	ETextureFormat Format = ETextureFormat::RGBA8UNorm;
	bool bSRGB = false; // Texture Color Space
};
