#include "Component/Mesh/StaticMeshComponent.h"

#include "Asset/AssetManager.h"
#include "Core/Assert.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"

#include <algorithm>

UStaticMeshComponent::UStaticMeshComponent(const char* DefaultAssetPath)
{
	panicf(GAssetManager, "기본 Static Mesh를 설정하려면 Asset Manager가 먼저 생성되어야 한다.");
	UStaticMesh* DefaultStaticMesh = GAssetManager->LoadStaticMesh(DefaultAssetPath);
	panicf(DefaultStaticMesh, "기본 Static Mesh를 찾을 수 없다. AssetPath={}", DefaultAssetPath);
	StaticMesh = DefaultStaticMesh;
}

void UStaticMeshComponent::SetStaticMesh(UStaticMesh* InStaticMesh)
{
	if (StaticMesh.Get() == InStaticMesh)
	{
		return;
	}

	StaticMesh = InStaticMesh;
	MarkPrimitiveSceneProxy();
}

void UStaticMeshComponent::SetMaterial(SIZE_T MaterialIndex, UMaterialInterface* InMaterial)
{
	if (MaterialIndex >= OverrideMaterials.size())
	{
		if (!InMaterial)
		{
			return;
		}
		OverrideMaterials.resize(MaterialIndex + 1);
	}
	if (OverrideMaterials[MaterialIndex].Get() == InMaterial)
	{
		return;
	}
	OverrideMaterials[MaterialIndex] = InMaterial;
	while (!OverrideMaterials.empty() && !OverrideMaterials.back())
	{
		OverrideMaterials.pop_back();
	}
	MarkPrimitiveSceneProxy();
}

UMaterialInterface* UStaticMeshComponent::GetMaterial(SIZE_T MaterialIndex) const
{
	if (MaterialIndex < OverrideMaterials.size() && OverrideMaterials[MaterialIndex])
	{
		return OverrideMaterials[MaterialIndex].Get();
	}
	return StaticMesh ? StaticMesh->GetMaterial(MaterialIndex) : nullptr;
}

SIZE_T UStaticMeshComponent::GetMaterialCount() const
{
	return std::max(OverrideMaterials.size(), StaticMesh ? StaticMesh->GetMaterialCount() : SIZE_T{ 0 });
}

std::unique_ptr<FPrimitiveSceneProxy> UStaticMeshComponent::CreatePrimitiveSceneProxy() const
{
	return std::make_unique<FStaticMeshSceneProxy>(*this);
}
