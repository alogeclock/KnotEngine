#pragma once

#include "EngineAPI.h"

#include "Core/CoreTypes.h"

class UStaticMesh;
class UMaterial;
class UTexture2D;
class FAssetManager;
class FMemoryReader;
enum class EAssetType : uint32;

// .kasset의 형식 검증, Payload 역직렬화와 UObject Asset 생성을 담당한다.
class ENGINE_API FAssetBinaryLoader final
{
public:
	UStaticMesh* LoadStaticMesh(const FString& AssetPath, FAssetManager& AssetManager) const;
	UMaterial* LoadMaterial(const FString& AssetPath, FAssetManager& AssetManager) const;
	UTexture2D* LoadTexture2D(const FString& AssetPath) const;

private:
	static TArray<uint8> LoadAssetFile(const FString& AssetPath);
	static bool ReadAssetHeader(FMemoryReader& Reader, SIZE_T FileSize, EAssetType ExpectedType, uint32 ExpectedPayloadVersion);
};
