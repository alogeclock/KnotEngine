#include "Component/Mesh/GeometryMeshComponent.h"

#include "Asset/AssetManager.h"
#include "Core/Assert.h"

UGeometryMeshComponent::UGeometryMeshComponent(EGeometryMeshType MeshType)
{
	checkf(GAssetManager, "Asset Manager 생성 전에 Geometry Mesh Component를 생성할 수 없다.");
	Mesh = GAssetManager->GetOrCreateGeometryMesh(MeshType);
	panic(Mesh.Get());
}
