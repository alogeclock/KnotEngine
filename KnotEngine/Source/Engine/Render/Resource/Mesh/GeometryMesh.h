#pragma once

#include "EngineAPI.h"
#include "Core/Geometry/AABB.h"
#include "Render/Resource/Mesh/MeshBuffer.h"
#include "Render/Resource/Mesh/Vertex.h"

#include <span>

class IRenderDevice;

// 디버그 도형의 CPU 원본 데이터와 GPU Mesh Buffer를 함께 소유하는 Renderer 내부 메시다.
class ENGINE_API FGeometryMesh
{
public:
	FGeometryMesh() = default;
	~FGeometryMesh() = default;

	FGeometryMesh(const FGeometryMesh&) = delete;
	FGeometryMesh& operator=(const FGeometryMesh&) = delete;
	FGeometryMesh(FGeometryMesh&&) = delete;
	FGeometryMesh& operator=(FGeometryMesh&&) = delete;

	void Initialize(std::span<const FGeometryVertex> InVertices, std::span<const uint32> InIndices);
	bool InitResources(IRenderDevice& RenderDevice);
	void Release();

	const TArray<FGeometryVertex>& GetVertices() const { return Vertices; }
	const TArray<uint32>& GetIndices() const { return Indices; }
	const FAABB& GetLocalBounds() const { return LocalBounds; }
	const FMeshBuffer& GetMeshBuffer() const { return MeshBuffer; }

private:
	TArray<FGeometryVertex> Vertices;
	TArray<uint32> Indices;
	FAABB LocalBounds;
	FMeshBuffer MeshBuffer;
};
