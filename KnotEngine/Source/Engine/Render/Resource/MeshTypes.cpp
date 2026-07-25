#include "Render/Resource/MeshTypes.h"

#include "Render/Resource/MeshResources.h"

#include <limits>

FGeometryMesh::FGeometryMesh() = default;

FGeometryMesh::~FGeometryMesh()
{
    Release();
}

FGeometryMesh::FGeometryMesh(FGeometryMesh&&) noexcept = default;
FGeometryMesh& FGeometryMesh::operator=(FGeometryMesh&&) noexcept = default;

// C 배열과 TArray 등 연속 메모리를 소유권 이전 없이 크기와 함께 받기 위해 span을 사용한다.
// 입력 데이터는 함수 안에서 CPU Mesh 배열로 복사하므로 span은 호출 중에만 유효하면 된다.
void FGeometryMesh::SetData(std::span<const FGeometryVertex> InVertices, std::span<const uint32> InIndices)
{
    Release();

    Vertices.assign(InVertices.begin(), InVertices.end());
    Indices.assign(InIndices.begin(), InIndices.end());
    bUploaded = false;
}

bool FGeometryMesh::Upload(URenderer& Renderer)
{
    Release();

    const FVertexLayout& VertexLayout = FGeometryVertex::GetVertexLayout();
    if (Vertices.empty() ||
        Vertices.size() > (std::numeric_limits<uint32>::max)() ||
        VertexLayout.Stride != sizeof(FGeometryVertex))
    {
        return false;
    }

    const auto* VertexBytes = reinterpret_cast<const uint8*>(Vertices.data());
    const FMeshDataView UploadData = {
        std::span<const uint8>(VertexBytes, Vertices.size() * sizeof(FGeometryVertex)),
        std::span<const uint32>(Indices.data(), Indices.size()),
        &VertexLayout,
        static_cast<uint32>(Vertices.size())
    };

    MeshBuffer = std::make_unique<FMeshBuffer>();
    if (!MeshBuffer->Initialize(Renderer, UploadData))
    {
        MeshBuffer.reset();
        return false;
    }

    bUploaded = true;
    return true;
}

void FGeometryMesh::Release()
{
    if (MeshBuffer)
    {
        MeshBuffer->Release();
        MeshBuffer.reset();
    }

    bUploaded = false;
}
