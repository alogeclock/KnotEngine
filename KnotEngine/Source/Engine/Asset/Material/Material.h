#pragma once

#include "Asset/Material/MaterialInterface.h"
#include "Asset/Material/MaterialParameters.h"
#include "Core/Archive/Archive.h"
#include "Render/Resource/Material/Material.h"

class FAssetBinaryLoader;
class FReferenceCollector;

// Material .kasset Payload의 Render State와 Parameter 배열 크기를 저장한다.
struct FMaterialPayloadHeader
{
	inline static constexpr uint32 CurrentVersion = 2;

	EMaterialBlendMode BlendMode;
	EDepthMode DepthMode;
	ECullMode CullMode;

	uint8 Reserved = 0;

	uint32 ScalarParameterCount;
	uint32 VectorParameterCount;
	uint32 TextureParameterCount;
};
static_assert(sizeof(FMaterialPayloadHeader) == 16);

inline FArchive& operator<<(FArchive& Ar, FMaterialPayloadHeader& Header)
{
	Ar << Header.BlendMode;
	Ar << Header.DepthMode;
	Ar << Header.CullMode;
	Ar << Header.Reserved;
	Ar << Header.ScalarParameterCount;
	Ar << Header.VectorParameterCount;
	Ar << Header.TextureParameterCount;
	return Ar;
}

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
		const FAssetId& InAssetId,
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
