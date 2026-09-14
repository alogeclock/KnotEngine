#pragma once

#include "EngineAPI.h"

#include "Object/Object.h"
#include "Render/Resource/MeshTypes.h"

enum class EGeometryMeshType : uint8
{
	Quad,
	Sphere,
	Cube,
};

// Engine 기본 도형 하나를 나타내는 공유 UObject Asset.
// 실제 CPU/GPU Mesh 데이터는 내부 FGeometryMesh가 소유한다.
UCLASS()
class ENGINE_API UGeometryMesh final : public UObject
{
	GENERATED_CLASS(UGeometryMesh, UObject)

public:
	void Initialize(EGeometryMeshType InMeshType);

	EGeometryMeshType GetMeshType() const { return MeshType; }
	FGeometryMesh& GetGeometryMesh() { return GeometryMesh; }
	const FGeometryMesh& GetGeometryMesh() const { return GeometryMesh; }

private:
	EGeometryMeshType MeshType = EGeometryMeshType::Quad;
	FGeometryMesh GeometryMesh;
};
