#include "Asset/Material/Material.h"

#include "Object/ReferenceCollector.h"

// 이름이 일치하는 기본 Scalar Parameter 값을 반환한다.
bool UMaterial::GetScalarParameterValue(const FName& Name, float& OutValue) const
{
	for (const FScalarMaterialParameter& Parameter : ScalarParameters)
	{
		if (Parameter.Name == Name)
		{
			OutValue = Parameter.Value;
			return true;
		}
	}
	return false;
}

// 이름이 일치하는 기본 Vector Parameter 값을 반환한다.
bool UMaterial::GetVectorParameterValue(const FName& Name, FVector4& OutValue) const
{
	for (const FVectorMaterialParameter& Parameter : VectorParameters)
	{
		if (Parameter.Name == Name)
		{
			OutValue = Parameter.Value;
			return true;
		}
	}
	return false;
}

// 이름이 일치하는 기본 Texture Parameter를 반환한다.
const FTextureMaterialParameter* UMaterial::FindTextureParameter(const FName& Name) const
{
	for (const FTextureMaterialParameter& Parameter : TextureParameters)
	{
		if (Parameter.Name == Name)
		{
			return &Parameter;
		}
	}
	return nullptr;
}

// Material 렌더 정의와 기본 Parameter를 검증해 UObject Asset에 저장한다.
bool UMaterial::Initialize(
	const FAssetId& InAssetId,
	FString InAssetPath,
	FMaterial&& InMaterial,
	TArray<FScalarMaterialParameter>&& InScalarParameters,
	TArray<FVectorMaterialParameter>&& InVectorParameters,
	TArray<FTextureMaterialParameter>&& InTextureParameters)
{
	if (!InMaterial.IsValid() || !Super::Initialize(InAssetId, std::move(InAssetPath)))
	{
		return false;
	}

	Material = std::move(InMaterial);
	ScalarParameters = std::move(InScalarParameters);
	VectorParameters = std::move(InVectorParameters);
	TextureParameters = std::move(InTextureParameters);
	return true;
}

// 기본 Texture Parameter가 참조하는 Texture Asset을 GC 도달 가능 객체에 추가한다.
void UMaterial::AddReferencedObjects(FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
	for (const FTextureMaterialParameter& Parameter : TextureParameters)
	{
		Collector.AddReferencedObject(Parameter.Texture);
	}
}
