#include "Asset/Material/MaterialInterface.h"

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
