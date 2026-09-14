#include "Component/Mesh/GeometryMeshComponent.h"

#include "Asset/ResourceManager.h"
#include "Core/Assert.h"

UGeometryMeshComponent::UGeometryMeshComponent(EGeometryMeshType MeshType)
{
	checkf(GResourceManager, "Resource Manager 생성 전에 Geometry Mesh Component를 생성할 수 없다.");
	Mesh = GResourceManager->GetOrCreateGeometryMesh(MeshType);
	panic(Mesh);
}
