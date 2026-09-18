#pragma once

#include "EngineAPI.h"

#include "Asset/Asset/Asset.h"
#include "Asset/Material/MaterialParameters.h"

class FMaterial;
struct FMaterialParameterLayout;

// Static Mesh Material Slot이 Base Material과 Material Instance를 동일하게 참조하기 위한 UObject 인터페이스다.
UCLASS()
class ENGINE_API UMaterialInterface : public UAsset
{
	GENERATED_CLASS(UMaterialInterface, UAsset)

public:
	EAssetType GetAssetType() const override { return EAssetType::Material; }
	virtual const FMaterial* GetMaterial() const = 0;

	virtual bool GetScalarParameterValue(const FName& Name, float& OutValue) const = 0;
	virtual bool GetVectorParameterValue(const FName& Name, FVector4& OutValue) const = 0;
	virtual const FTextureMaterialParameter* FindTextureParameter(const FName& Name) const = 0;

	void PackMaterialConstants(const FMaterialParameterLayout& Layout, TArray<uint8>& OutData) const;

protected:
	bool Initialize(const FAssetId& InAssetId, FString InAssetPath);
};
