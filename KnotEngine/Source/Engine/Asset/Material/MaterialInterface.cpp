#include "Asset/Material/MaterialInterface.h"

#include "Core/Assert.h"

#include <cstring>

// Material Interface를 식별하는 논리 Asset 경로를 검증해 저장한다.
bool UMaterialInterface::Initialize(const FAssetId& InAssetId, FString InAssetPath)
{
	if (!InitializeAsset(InAssetId, std::move(InAssetPath)))
	{
		return false;
	}
	Revision = 1;
	return true;
}

// Reflection Layout의 Offset/Size에 맞춰 Material 및 Instance Parameter 값을 연속 Constant Buffer 데이터로 패킹한다.
void UMaterialInterface::PackMaterialConstants(std::span<const FMaterialParameterDesc> Parameters, uint32 ConstantBufferSize, TArray<uint8>& OutData) const
{
	OutData.assign(ConstantBufferSize, 0);
	for (const FMaterialParameterDesc& Parameter : Parameters)
	{
		checkf(Parameter.Offset + Parameter.Size <= OutData.size(), "Material Parameter Layout이 Constant Buffer 범위를 벗어났다.");
		uint8* Destination = OutData.data() + Parameter.Offset;
		if (Parameter.Type == EMaterialParameterType::Scalar)
		{
			float Value = 0.0f;
			if (GetScalarParameterValue(Parameter.Name, Value))
			{
				check(Parameter.Size == sizeof(float));
				std::memcpy(Destination, &Value, sizeof(Value));
			}
			continue;
		}

		FVector4 Value;
		if (GetVectorParameterValue(Parameter.Name, Value))
		{
			check(Parameter.Size <= sizeof(Value));
			std::memcpy(Destination, Value.Data, Parameter.Size);
		}
	}
}
