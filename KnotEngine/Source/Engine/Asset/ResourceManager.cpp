#include "Asset/ResourceManager.h"

#include "Core/Assert.h"
#include "Render/Resource/MeshResources.h"
#include "Render/Resource/MeshTypes.h"
#include "Render/Resource/VertexTypes.h"
#include "Render/RHI/RenderDevice.h"

#include <limits>

FResourceManager* GResourceManager = nullptr;

FResourceManager::FResourceManager(IRenderDevice& InRenderDevice)
	: RenderDevice(InRenderDevice)
{
}

FResourceManager::~FResourceManager()
{
	Release();
}

void FResourceManager::Create()
{
	Release();
	checkf(!GResourceManager, "Resource Manager가 이미 생성되어 있다.");
	GResourceManager = this;
}

std::shared_ptr<FGeometryMesh> FResourceManager::GetOrCreateGeometryMesh(EGeometryMeshType MeshType)
{
	check(GResourceManager == this);
	const auto It = GeometryMeshCache.find(MeshType);
	if (It != GeometryMeshCache.end())
	{
		return It->second;
	}

	std::shared_ptr<FGeometryMesh> Mesh = FGeometryMesh::Create(MeshType);
	panic(Mesh);
	GeometryMeshCache.emplace(MeshType, Mesh);
	return Mesh;
}

FMeshBuffer* FResourceManager::GetOrCreateMeshBuffer(FGeometryMesh& Mesh)
{
	check(GResourceManager == this);
	if (Mesh.MeshBuffer.IsValid())
	{
		return &Mesh.MeshBuffer;
	}

	const TArray<FGeometryVertex>& Vertices = Mesh.GetVertices();
	checkf(!Vertices.empty() && Vertices.size() <= (std::numeric_limits<uint32>::max)(),
		"Geometry Mesh의 Vertex 데이터가 유효하지 않다. VertexCount={}", Vertices.size());
	const auto* VertexBytes = reinterpret_cast<const uint8*>(Vertices.data());
	const FMeshDataView DataView = {
		std::span<const uint8>(VertexBytes, Vertices.size() * sizeof(FGeometryVertex)),
		std::span<const uint32>(Mesh.GetIndices().data(), Mesh.GetIndices().size()),
		&FGeometryVertex::GetVertexLayout(),
		static_cast<uint32>(Vertices.size())
	};

	panicf(Mesh.MeshBuffer.Initialize(RenderDevice, DataView), "Geometry Mesh의 GPU Buffer 생성에 실패했다.");
	return &Mesh.MeshBuffer;
}

void FResourceManager::Release()
{
	if (GResourceManager != this)
	{
		return;
	}

	for (const auto& Entry : GeometryMeshCache)
	{
		const std::shared_ptr<FGeometryMesh>& Mesh = Entry.second;
		if (Mesh)
		{
			Mesh->Release();
		}
	}
	GeometryMeshCache.clear();
	GResourceManager = nullptr;
}
