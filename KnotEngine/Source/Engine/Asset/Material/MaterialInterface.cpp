#include "Asset/Material/MaterialInterface.h"

#include "Core/Assert.h"
#include "Render/Material/Material.h"

#include <cstring>

// Material Interface를 식별하는 논리 Asset 경로를 검증해 저장한다.
bool UMaterialInterface::Initialize(FString InAssetPath)
{
	if (InAssetPath.empty())
	{
		return false;
	}
	AssetPath = std::move(InAssetPath);
	return true;
}

// Reflection Layout의 Offset/Size에 맞춰 Material 및 Instance Parameter 값을 연속 Constant Buffer 데이터로 패킹한다.
void UMaterialInterface::PackMaterialConstants(const FMaterialParameterLayout& Layout, TArray<uint8>& OutData) const
{
	OutData.assign(Layout.ConstantBufferSize, 0);
	for (const FMaterialParameterDesc& Parameter : Layout.Parameters)
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
