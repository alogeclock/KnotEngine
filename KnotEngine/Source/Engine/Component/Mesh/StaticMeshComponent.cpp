#include "Component/Mesh/StaticMeshComponent.h"

#include "Asset/AssetManager.h"
#include "Core/Assert.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"

UStaticMeshComponent::UStaticMeshComponent(const char* DefaultAssetPath)
{
	panicf(GAssetManager, "기본 Static Mesh를 설정하려면 Asset Manager가 먼저 생성되어야 한다.");
	UStaticMesh* DefaultStaticMesh = GAssetManager->FindStaticMesh(DefaultAssetPath);
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

std::unique_ptr<FPrimitiveSceneProxy> UStaticMeshComponent::CreatePrimitiveSceneProxy() const
{
	return std::make_unique<FStaticMeshSceneProxy>(*this);
}
