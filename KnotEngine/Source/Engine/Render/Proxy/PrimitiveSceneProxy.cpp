#include "Render/Proxy/PrimitiveSceneProxy.h"

#include "Render/Renderer.h"
#include "Render/Resource/MaterialResource.h"
#include "Render/Resource/Mesh/StaticMeshResource.h"
#include "Render/Scene/SceneView.h"

#include <algorithm>
#include <cmath>

void FPrimitiveSceneProxy::ApplyPrimitiveData(ERenderCommandType Type, const FPrimitiveRenderData& RenderData)
{
	if (HasRenderCommand(Type, ERenderCommandType::Transform) || HasRenderCommand(Type, ERenderCommandType::Mesh))
	{
		WorldMatrix = RenderData.WorldMatrix;
	}
	if (HasRenderCommand(Type, ERenderCommandType::Mesh))
	{
		LocalBounds = RenderData.LocalBounds;
	}
	if (HasRenderCommand(Type, ERenderCommandType::Transform) || HasRenderCommand(Type, ERenderCommandType::Mesh))
	{
		WorldBounds = LocalBounds.IsValid() ? LocalBounds.Transform(WorldMatrix) : FAABB();
		WorldBoundsRadius = WorldBounds.IsValid() ? WorldBounds.GetExtent().Size() : 0.0f;
	}
	if (HasRenderCommand(Type, ERenderCommandType::Visibility))
	{
		bVisible = RenderData.bVisible;
		bSelected = RenderData.bSelected;
	}
}

FStaticMeshSceneProxy::FStaticMeshSceneProxy(const FPrimitiveRenderData& RenderData)
{
	WorldMatrix = RenderData.WorldMatrix;
	LocalBounds = RenderData.LocalBounds;
	bVisible = RenderData.bVisible;
	bSelected = RenderData.bSelected;
	bLODEnable = RenderData.bLODEnable;
	WorldBounds = LocalBounds.IsValid() ? LocalBounds.Transform(WorldMatrix) : FAABB();
	WorldBoundsRadius = WorldBounds.IsValid() ? WorldBounds.GetExtent().Size() : 0.0f;
}

void FStaticMeshSceneProxy::Apply(ERenderCommandType Type, const FPrimitiveRenderData& RenderData, FRenderer& Renderer)
{
	ApplyPrimitiveData(Type, RenderData);
	if (HasRenderCommand(Type, ERenderCommandType::Mesh))
	{
		bLODEnable = RenderData.bLODEnable;
		MeshResource = RenderData.MeshAssetId.IsValid() ? Renderer.FindStaticMeshResource(RenderData.MeshAssetId) : nullptr;
		check(!RenderData.MeshAssetId.IsValid() || MeshResource);
	}
	if (HasRenderCommand(Type, ERenderCommandType::Material))
	{
		DefaultMaterial = &Renderer.GetDefaultMaterialResource();
		Materials.clear();
		Materials.reserve(RenderData.MaterialAssetIds.size());
		for (const FAssetId& MaterialAssetId : RenderData.MaterialAssetIds)
		{
			const FMaterialResource* Material = MaterialAssetId.IsValid() ? Renderer.FindMaterialResource(MaterialAssetId) : DefaultMaterial;
			check(Material);
			Materials.push_back(Material);
		}
	}
}

const FMaterialResource& FStaticMeshSceneProxy::GetMaterial(SIZE_T MaterialIndex) const
{
	check(DefaultMaterial);
	return MaterialIndex < Materials.size() ? *Materials[MaterialIndex] : *DefaultMaterial;
}

SIZE_T FStaticMeshSceneProxy::SelectLOD(const FSceneView& View) const
{
	check(MeshResource && MeshResource->GetLODCount() > 0);
	if (View.ForcedLODIndex >= 0)
	{
		return std::min(static_cast<SIZE_T>(View.ForcedLODIndex), MeshResource->GetLODCount() - 1);
	}
	if (!bLODEnable)
	{
		return 0;
	}
	if (MeshResource->GetLODCount() == 1 || std::fabs(View.ProjectionMatrix.M[2][3]) <= KMath::Epsilon)
	{
		return 0;
	}

	const float DistanceSquared = std::max(FVector::DistSquared(WorldBounds.GetCenter(), View.ViewOrigin), KMath::Epsilon * KMath::Epsilon);
	const float ProjectedRadiusScale = WorldBoundsRadius * std::fabs(View.ProjectionMatrix.M[1][1]);
	const float ProjectedRadiusScaleSquared = ProjectedRadiusScale * ProjectedRadiusScale;
	static constexpr SIZE_T MaximumLODCount = 5;
	const SIZE_T LODCount = std::min(MeshResource->GetLODCount(), MaximumLODCount);
	SIZE_T LODIndex = 0;
	while (LODIndex + 1 < LODCount)
	{
		const float Threshold = View.LODSteps[LODIndex];
		if (ProjectedRadiusScaleSquared >= Threshold * Threshold * DistanceSquared)
		{
			break;
		}
		++LODIndex;
	}
	return LODIndex;
}
