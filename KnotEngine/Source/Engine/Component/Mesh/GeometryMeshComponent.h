#pragma once

#include "Asset/GeometryMesh.h"
#include "Component/Mesh/MeshComponent.h"

// Engine 기본 도형의 CPU Geometry를 생성하는 MeshComponent 기반 클래스.
UCLASS()
class ENGINE_API UGeometryMeshComponent : public UMeshComponent
{
	GENERATED_CLASS(UGeometryMeshComponent, UMeshComponent)

protected:
	explicit UGeometryMeshComponent(EGeometryMeshType MeshType);
};
