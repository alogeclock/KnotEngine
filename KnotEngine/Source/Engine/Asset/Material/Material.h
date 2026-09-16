#pragma once

#include "Asset/Material/MaterialInterface.h"
#include "Asset/Material/MaterialParameters.h"
#include "Render/Resource/Material.h"

class FAssetBinaryLoader;
class FReferenceCollector;

// Material 계열 .kasset의 종류와 Payload 배열 크기를 저장하는 고정 크기 헤더다.
struct FMaterialBinaryHeader
{
	inline static constexpr char MagicValue[4] = { 'K', 'M', 'A', 'T' };
	inline static constexpr uint32 CurrentVersion = 1;

	char Magic[4];
	uint32 Version;
	uint32 ScalarParameterCount;
	uint32 VectorParameterCount;
	uint32 TextureParameterCount;
};
static_assert(sizeof(FMaterialBinaryHeader) == 20);

// Shader와 Pipeline 정의 및 변경되지 않는 기본 Parameter를 소유하는 Material Asset이다.
UCLASS()
class ENGINE_API UMaterial final : public UMaterialInterface
{
	GENERATED_CLASS(UMaterial, UMaterialInterface)

public:
	const FMaterial* GetMaterial() const override { return &Material; }
	bool GetScalarParameterValue(const FName& Name, float& OutValue) const override;
	bool GetVectorParameterValue(const FName& Name, FVector4& OutValue) const override;

	// TODO: 셰이더 리플렉션 및 Parameter 확장 구현 후 재검토
	const FTextureMaterialParameter* FindTextureParameter(const FName& Name) const override;
	const TArray<FScalarMaterialParameter>& GetScalarParameters() const { return ScalarParameters; }
	const TArray<FVectorMaterialParameter>& GetVectorParameters() const { return VectorParameters; }
	const TArray<FTextureMaterialParameter>& GetTextureParameters() const { return TextureParameters; }

	void AddReferencedObjects(FReferenceCollector& Collector) override;

private:
	friend class FAssetBinaryLoader;
	bool Initialize(
		FString InAssetPath,
		FMaterial&& InMaterial,
		TArray<FScalarMaterialParameter>&& InScalarParameters,
		TArray<FVectorMaterialParameter>&& InVectorParameters,
		TArray<FTextureMaterialParameter>&& InTextureParameters);

	FMaterial Material;
	TArray<FScalarMaterialParameter> ScalarParameters;
	TArray<FVectorMaterialParameter> VectorParameters;
	TArray<FTextureMaterialParameter> TextureParameters;
};
