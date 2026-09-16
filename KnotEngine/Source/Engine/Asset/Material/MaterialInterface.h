#pragma once

#include "EngineAPI.h"

#include "Asset/Material/MaterialParameters.h"
#include "Object/Object.h"

class FMaterial;

// Static Mesh Material Slot이 Base Material과 Material Instance를 동일하게 참조하기 위한 UObject 인터페이스다.
UCLASS()
class ENGINE_API UMaterialInterface : public UObject
{
	GENERATED_CLASS(UMaterialInterface, UObject)

public:
	const FString& GetAssetPath() const { return AssetPath; }
	virtual const FMaterial* GetMaterial() const = 0;

	virtual bool GetScalarParameterValue(const FName& Name, float& OutValue) const = 0;
	virtual bool GetVectorParameterValue(const FName& Name, FVector4& OutValue) const = 0;
	virtual const FTextureMaterialParameter* FindTextureParameter(const FName& Name) const = 0;

protected:
	bool Initialize(FString InAssetPath);

private:
	UPROPERTY(NoEdit) FString AssetPath;
};
