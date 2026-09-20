#include "Component/Mesh/StaticMeshComponent.h"

#include "Asset/AssetManager.h"
#include "Core/Assert.h"
#include "Object/Property.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Component/TransformComponent.h"
#include "World/Node.h"

#include <algorithm>

UStaticMeshComponent::UStaticMeshComponent(const FAssetId& DefaultAssetId)
{
	panicf(GAssetManager, "기본 Static Mesh를 설정하려면 Asset Manager가 먼저 생성되어야 한다.");
	UStaticMesh* DefaultStaticMesh = GAssetManager->LoadStaticMesh(DefaultAssetId);
	panicf(DefaultStaticMesh, "기본 Static Mesh를 찾을 수 없다. AssetId={}", DefaultAssetId.ToString());
	StaticMesh = DefaultStaticMesh;
}

void UStaticMeshComponent::SetStaticMesh(UStaticMesh* InStaticMesh)
{
	if (StaticMesh.Get() == InStaticMesh)
	{
		return;
	}

	StaticMesh = InStaticMesh;
	OverrideMaterials.clear();
	EnqueueRenderCommand(ERenderCommandType::Mesh | ERenderCommandType::Material);
}

// Inspector에서 Static Mesh가 교체되면 이전 Mesh의 Material Override를 제거한다.
void UStaticMeshComponent::PostEditProperty(const FProperty& Property)
{
	static const FName StaticMeshPropertyName("StaticMesh");
	if (Property.GetFName() == StaticMeshPropertyName)
	{
		OverrideMaterials.clear();
	}
	Super::PostEditProperty(Property);
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
	EnqueueRenderCommand(ERenderCommandType::Material);
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

FPrimitiveRenderData UStaticMeshComponent::BuildPrimitiveRenderData(ERenderCommandType Type) const
{
	FPrimitiveRenderData RenderData;
	if (HasRenderCommand(Type, ERenderCommandType::Transform) || HasRenderCommand(Type, ERenderCommandType::Mesh))
	{
		RenderData.WorldMatrix = GetTransform().GetWorldMatrix();
	}

	if (HasRenderCommand(Type, ERenderCommandType::Visibility))
	{
		RenderData.bVisible = IsVisible();
		RenderData.bSelected = GetOwner().IsSelected();
	}

	UStaticMesh* MeshAsset = StaticMesh.Get();
	if (HasRenderCommand(Type, ERenderCommandType::Mesh) && MeshAsset)
	{
		RenderData.Mesh = &MeshAsset->GetMeshData();
		RenderData.MeshAssetId = MeshAsset->GetAssetId();
		RenderData.MeshRevision = MeshAsset->GetRevision();
		RenderData.LocalBounds = RenderData.Mesh->GetLocalBounds();
		RenderData.bLODEnable = bLODEnable;
	}

	if (HasRenderCommand(Type, ERenderCommandType::Material))
	{
		const SIZE_T MaterialCount = std::max<SIZE_T>(GetMaterialCount(), 1);
		RenderData.Materials.reserve(MaterialCount);
		for (SIZE_T MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
		{
			RenderData.Materials.push_back(GetMaterial(MaterialIndex));
		}
	}

	return RenderData;
}

std::unique_ptr<FPrimitiveSceneProxy> UStaticMeshComponent::CreatePrimitiveSceneProxy(const FPrimitiveRenderData& RenderData) const
{
	return std::make_unique<FStaticMeshSceneProxy>(RenderData);
}
