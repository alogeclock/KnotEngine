#pragma once

#include "Asset/Material/MaterialInterface.h"
#include "Asset/Material/MaterialParameters.h"
#include "Object/ObjectPtr.h"

class FAssetBinaryLoader;
class FReferenceCollector;
class UMaterial;

// 부모 UMaterial의 렌더 정의를 공유하고 Parameter 값만 재정의하는 Material Instance Asset이다.
UCLASS()
class ENGINE_API UMaterialInstance final : public UMaterialInterface
{
	GENERATED_CLASS(UMaterialInstance, UMaterialInterface)

public:
	const FMaterial* GetMaterial() const override;

	void Copy(
		TArray<FScalarMaterialParameter>& OutScalars,
		TArray<FVectorMaterialParameter>& OutVectors,
		TArray<FTextureMaterialParameter>& OutTextures) const override;

	bool GetScalarParameterValue(const FName& Name, float& OutValue) const override;
	bool GetVectorParameterValue(const FName& Name, FVector4& OutValue) const override;
	const FTextureMaterialParameter* FindTextureParameter(const FName& Name) const override;

	UMaterial* GetParent() const { return Parent.Get(); }
	const TArray<FScalarMaterialParameter>& GetScalarParameters() const { return ScalarParameters; }
	const TArray<FVectorMaterialParameter>& GetVectorParameters() const { return VectorParameters; }
	const TArray<FTextureMaterialParameter>& GetTextureParameters() const { return TextureParameters; }

	void AddReferencedObjects(FReferenceCollector& Collector) override;

private:
	friend class FAssetBinaryLoader;
	bool Initialize(
	    const FAssetId& InAssetId,
	    FString InAssetPath,
	    UMaterial* InParent,
	    TArray<FScalarMaterialParameter>&& InScalarParameters,
	    TArray<FVectorMaterialParameter>&& InVectorParameters,
	    TArray<FTextureMaterialParameter>&& InTextureParameters);

	TObjectPtr<UMaterial> Parent;
	TArray<FScalarMaterialParameter> ScalarParameters;
	TArray<FVectorMaterialParameter> VectorParameters;
	TArray<FTextureMaterialParameter> TextureParameters;
};
