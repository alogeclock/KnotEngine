#include "Asset/Material/MaterialInstance.h"

#include "Asset/Material/Material.h"
#include "Object/ReferenceCollector.h"

#include <algorithm>

// Material Instance가 공유하는 부모 Material의 렌더 정의를 반환한다.
const FMaterial* UMaterialInstance::GetMaterial() const
{
	return Parent ? Parent->GetMaterial() : nullptr;
}

// Instance Override를 먼저 찾고 없으면 부모 Material의 Scalar Parameter를 반환한다.
bool UMaterialInstance::GetScalarParameterValue(const FName& Name, float& OutValue) const
{
	for (const FScalarMaterialParameter& Parameter : ScalarParameters)
	{
		if (Parameter.Name == Name)
		{
			OutValue = Parameter.Value;
			return true;
		}
	}
	return Parent && Parent->GetScalarParameterValue(Name, OutValue);
}

// Instance Override를 먼저 찾고 없으면 부모 Material의 Vector Parameter를 반환한다.
bool UMaterialInstance::GetVectorParameterValue(const FName& Name, FVector4& OutValue) const
{
	for (const FVectorMaterialParameter& Parameter : VectorParameters)
	{
		if (Parameter.Name == Name)
		{
			OutValue = Parameter.Value;
			return true;
		}
	}
	return Parent && Parent->GetVectorParameterValue(Name, OutValue);
}

// Instance Override를 먼저 찾고 없으면 부모 Material의 Texture Parameter를 반환한다.
const FTextureMaterialParameter* UMaterialInstance::FindTextureParameter(const FName& Name) const
{
	for (const FTextureMaterialParameter& Parameter : TextureParameters)
	{
		if (Parameter.Name == Name)
		{
			return &Parameter;
		}
	}
	return Parent ? Parent->FindTextureParameter(Name) : nullptr;
}

void UMaterialInstance::Copy(
	TArray<FScalarMaterialParameter>& OutScalars,
	TArray<FVectorMaterialParameter>& OutVectors,
	TArray<FTextureMaterialParameter>& OutTextures) const
{
	OutScalars.clear();
	OutVectors.clear();
	OutTextures.clear();
	if (Parent)
	{
		Parent->Copy(OutScalars, OutVectors, OutTextures);
	}

	for (const FScalarMaterialParameter& Parameter : ScalarParameters)
	{
		const auto Existing = std::find_if(OutScalars.begin(), OutScalars.end(), [&Parameter](const FScalarMaterialParameter& Candidate)
		{
			return Candidate.Name == Parameter.Name;
		});
		if (Existing != OutScalars.end())
		{
			*Existing = Parameter;
		}
		else
		{
			OutScalars.push_back(Parameter);
		}
	}
	for (const FVectorMaterialParameter& Parameter : VectorParameters)
	{
		const auto Existing = std::find_if(OutVectors.begin(), OutVectors.end(), [&Parameter](const FVectorMaterialParameter& Candidate)
		{
			return Candidate.Name == Parameter.Name;
		});
		if (Existing != OutVectors.end())
		{
			*Existing = Parameter;
		}
		else
		{
			OutVectors.push_back(Parameter);
		}
	}
	for (const FTextureMaterialParameter& Parameter : TextureParameters)
	{
		const auto Existing = std::find_if(OutTextures.begin(), OutTextures.end(), [&Parameter](const FTextureMaterialParameter& Candidate)
		{
			return Candidate.Name == Parameter.Name;
		});
		if (Existing != OutTextures.end())
		{
			*Existing = Parameter;
		}
		else
		{
			OutTextures.push_back(Parameter);
		}
	}
}

// 부모 Material과 Instance 전용 Parameter Override를 검증해 UObject Asset에 저장한다.
bool UMaterialInstance::Initialize(
	const FAssetId& InAssetId,
	FString InAssetPath,
	UMaterial* InParent,
	TArray<FScalarMaterialParameter>&& InScalarParameters,
	TArray<FVectorMaterialParameter>&& InVectorParameters,
	TArray<FTextureMaterialParameter>&& InTextureParameters)
{
	if (!InParent || !InParent->GetMaterial() || !InParent->GetMaterial()->IsValid() || !Super::Initialize(InAssetId, std::move(InAssetPath)))
	{
		return false;
	}

	Parent = InParent;
	ScalarParameters = std::move(InScalarParameters);
	VectorParameters = std::move(InVectorParameters);
	TextureParameters = std::move(InTextureParameters);
	return true;
}

// 부모 Material과 Override Texture가 참조하는 Asset을 GC 도달 가능 객체에 추가한다.
void UMaterialInstance::AddReferencedObjects(FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
	Collector.AddReferencedObject(Parent);
	for (const FTextureMaterialParameter& Parameter : TextureParameters)
	{
		Collector.AddReferencedObject(Parameter.Texture);
	}
}
