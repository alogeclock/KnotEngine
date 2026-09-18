#pragma once

#include "EngineAPI.h"

#include "Core/CoreTypes.h"

class UStaticMesh;
class UMaterial;
class UTexture2D;
class FAssetManager;
struct FAssetData;
struct FAssetFileHeader;
class FMemoryReader;
enum class EAssetType : uint32;

// .kasset의 형식 검증, Payload 역직렬화와 UObject Asset 생성을 담당한다.
class ENGINE_API FAssetBinaryLoader final
{
public:
	UStaticMesh* LoadStaticMesh(const FAssetData& Asset, FAssetManager& AssetManager) const;
	UMaterial* LoadMaterial(const FAssetData& Asset, FAssetManager& AssetManager) const;
	UTexture2D* LoadTexture2D(const FAssetData& Asset) const;

private:
	static TArray<uint8> LoadAssetFile(const FAssetData& Asset);
	static bool ReadAssetHeader(
		FMemoryReader& Reader,
		SIZE_T FileSize,
		const FAssetData& Asset,
		EAssetType ExpectedType,
		uint32 ExpectedPayloadVersion,
		FAssetFileHeader& OutHeader);
};
