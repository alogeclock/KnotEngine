#pragma once

#include "Asset/AssetTypes.h"
#include "Core/CoreTypes.h"

#include <filesystem>

struct FVector;
struct FSamplerDesc;
struct FStaticMeshVertex;
struct cgltf_accessor;
struct cgltf_data;
struct cgltf_primitive;
struct cgltf_sampler;

// 한 번의 Import로 생성된 개별 Runtime Asset의 논리 경로와 종류다.
struct FImportedAsset
{
	FString AssetPath;
	EAssetType Type = EAssetType::Unknown;
};

// Import 성공 여부와 생성된 Asset, 경고 및 오류를 함께 반환하는 결과다.
struct FAssetImportResult
{
	bool bSucceeded = false;
	TArray<FImportedAsset> ImportedAssets;
	TArray<FString> Warnings;
	FString Error;
};

// GLB Source를 Runtime 전용 Mesh, Material, Texture .kasset으로 변환하는 Editor 전용 Importer다.
class FAssetImporter final
{
public:
	FAssetImportResult ImportGLB(const std::filesystem::path& SourceFilePath, const FString& DestinationAssetPath, bool bCreateTypeFolders = true) const;

	bool ImportAllGLB() const;

private:
	struct FGLTFGuard
	{
		~FGLTFGuard();
		cgltf_data* Data = nullptr;
	};

	static FString Sanitize(const char* Name, const FString& Fallback);
	static FString Combine(const FString& Left, const FString& Right);

	static FVector ConvertPosition(const float Value[3]);
	static FVector ConvertDirection(const float Value[3]);

	static const cgltf_accessor* FindAttribute(const cgltf_primitive& Primitive, int Type, int Index = 0);
	static FSamplerDesc ConvertSampler(const cgltf_sampler* Sampler);

	static bool SaveAsset(const FString& AssetPath, EAssetType Type, uint32 PayloadVersion, const TArray<uint8>& PayloadBytes);
	static bool IsAssetUpToDate(const FString& AssetPath, const std::filesystem::file_time_type& SourceTimestamp, EAssetType ExpectedType, uint32 ExpectedPayloadVersion);

	static void GenerateTangents(TArray<FStaticMeshVertex>& Vertices, const TArray<uint32>& Indices);
	static void Normalize(TArray<FStaticMeshVertex>& Vertices, float MaximumSize);
};
