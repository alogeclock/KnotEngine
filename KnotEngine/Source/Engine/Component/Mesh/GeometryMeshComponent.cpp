#include "Component/Mesh/GeometryMeshComponent.h"

#include "Core/Assert.h"

UGeometryMeshComponent::UGeometryMeshComponent(EGeometryMeshType MeshType)
{
	Mesh = FGeometryMesh::Create(MeshType);
	panic(Mesh);
}
