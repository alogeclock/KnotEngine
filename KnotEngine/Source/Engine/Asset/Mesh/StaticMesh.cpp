#include "Asset/Mesh/StaticMesh.h"

bool UStaticMesh::Initialize(FString InAssetPath, FStaticMesh&& InRenderData)
{
	if (InAssetPath.empty() || !InRenderData.IsValid())
	{
		return false;
	}

	AssetPath = std::move(InAssetPath);
	RenderData = std::move(InRenderData);
	return true;
}
