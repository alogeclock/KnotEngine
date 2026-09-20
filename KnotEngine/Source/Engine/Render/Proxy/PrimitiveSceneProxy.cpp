#include "Render/Proxy/PrimitiveSceneProxy.h"

#include "Component/Mesh/StaticMeshComponent.h"
#include "Component/TransformComponent.h"
#include "Render/Resource/Mesh/Mesh.h"
#include "Render/Scene/SceneView.h"
#include "World/Node.h"

#include <algorithm>
#include <cmath>

FPrimitiveSceneProxy::FPrimitiveSceneProxy(const UPrimitiveComponent& InComponent)
	: Component(InComponent), bSelected(InComponent.GetOwner().IsSelected())
{
}

UNode& FPrimitiveSceneProxy::GetOwner() const
{
	return Component.GetOwner();
}

void FPrimitiveSceneProxy::UpdateBounds(const FAABB& InLocalBounds)
{
	// Component와의 friend 관계로 현재 상태를 읽는다. 갱신 중 임시 Proxy를 생성하지 않는다.
	WorldMatrix = Component.GetTransform().GetWorldMatrix();
	bVisible = Component.bVisible;
	LocalBounds = InLocalBounds;
	WorldBounds = LocalBounds.IsValid() ? LocalBounds.Transform(WorldMatrix) : FAABB();
	WorldBoundsRadius = WorldBounds.IsValid() ? WorldBounds.GetExtent().Size() : 0.0f;
	bDirty = false;
}

FStaticMeshSceneProxy::FStaticMeshSceneProxy(const UStaticMeshComponent& InComponent)
	: FPrimitiveSceneProxy(InComponent), MeshComponent(InComponent)
{
	Update();
}

void FStaticMeshSceneProxy::Update()
{
	UStaticMesh* StaticMesh = MeshComponent.GetStaticMesh();
	if (!bDirty && MeshRevision == (StaticMesh ? StaticMesh->GetRevision() : 0))
	{
		return;
	}

	Mesh = StaticMesh ? &StaticMesh->GetRenderData() : nullptr;
	MeshRevision = StaticMesh ? StaticMesh->GetRevision() : 0;
	bLODEnabled = MeshComponent.IsLODEnable();
	Materials.clear();
	if (Mesh && !Mesh->IsValid())
	{
		Mesh = nullptr;
	}
	if (Mesh)
	{
		Materials.resize(MeshComponent.GetMaterialCount());
		for (SIZE_T MaterialIndex = 0; MaterialIndex < Materials.size(); ++MaterialIndex)
		{
			Materials[MaterialIndex] = MeshComponent.GetMaterial(MaterialIndex);
		}
	}
	UpdateBounds(Mesh ? Mesh->GetLocalBounds() : FAABB());
}

SIZE_T FStaticMeshSceneProxy::SelectLOD(const FSceneView& View) const
{
	check(Mesh && Mesh->GetLODCount() > 0);
	if (!bLODEnabled)
	{
		return 0;
	}
	if (Mesh->GetLODCount() == 1 || std::fabs(View.ProjectionMatrix.M[2][3]) <= KMath::Epsilon)
	{
		return 0;
	}

	const float DistanceSquared = std::max(FVector::DistSquared(WorldBounds.GetCenter(), View.ViewOrigin), KMath::Epsilon * KMath::Epsilon);
	const float ProjectedRadiusScale = WorldBoundsRadius * std::fabs(View.ProjectionMatrix.M[1][1]);
	const float ProjectedRadiusScaleSquared = ProjectedRadiusScale * ProjectedRadiusScale;
	static constexpr SIZE_T MaximumLODCount = 5;
	const SIZE_T LODCount = std::min(Mesh->GetLODCount(), MaximumLODCount);
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
