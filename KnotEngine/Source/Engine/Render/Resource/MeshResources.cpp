#include "Render/Resource/MeshResources.h"

#include "Render/Renderer.h"

#include <limits>

FMeshBuffer::~FMeshBuffer()
{
    Release();
}

bool FMeshBuffer::Initialize(URenderer& Renderer, const FMeshDataView& InDataView)
{
    Release();

    if (!Validate(InDataView))
    {
        return false;
    }

    const FVertexLayout& UploadLayout = *InDataView.Layout;

    if (!Renderer.CreateVertexBuffer(VertexBuffer, InDataView.VertexBytes, InDataView.VertexCount, UploadLayout.Stride))
    {
        return false;
    }

    if (!InDataView.Indices.empty() &&
        !Renderer.CreateIndexBuffer(IndexBuffer, InDataView.Indices))
    {
        VertexBuffer.Release();
        return false;
    }

    VertexLayout = UploadLayout;
    bInitialized = true;
    return true;
}

bool FMeshBuffer::IsValid() const
{
    if (!IsInitialized() || !VertexBuffer.IsValid())
    {
        return false;
    }

    return GetIndexCount() == 0 || IndexBuffer.IsValid();
}

void FMeshBuffer::OnRelease()
{
    IndexBuffer.Release();
    VertexBuffer.Release();
    VertexLayout = {};
}

// 리소스 뷰의 Vertex Buffer, Index Buffer 정점 수 및 레이아웃, 배열 크기 등을 기반으로 버퍼가 유효한지 검증한다.
bool FMeshBuffer::Validate(const FMeshDataView& DataView)
{
    if (DataView.VertexCount == 0 || !DataView.Layout || DataView.Layout->Stride == 0 || DataView.Layout->Elements.empty())
    {
        return false;
    }

    const size_t ExpectedVertexByteCount = static_cast<size_t>(DataView.VertexCount) * DataView.Layout->Stride;
    if (DataView.VertexBytes.size() != ExpectedVertexByteCount)
    {
        return false;
    }

    if (DataView.Indices.size() > (std::numeric_limits<uint32>::max)())
    {
        return false;
    }

    for (size_t ElementIndex = 0; ElementIndex < DataView.Layout->Elements.size(); ++ElementIndex)
    {
        const FVertexElement& Element = DataView.Layout->Elements[ElementIndex];
        const uint16 FormatByteSize = GetVertexFormatBytes(Element.Format);
        const uint32 AttributeEnd = static_cast<uint32>(Element.Offset) + FormatByteSize;
        if (FormatByteSize == 0 || AttributeEnd > DataView.Layout->Stride)
        {
            return false;
        }

        for (size_t PreviousIndex = 0; PreviousIndex < ElementIndex; ++PreviousIndex)
        {
            const FVertexElement& PreviousElement = DataView.Layout->Elements[PreviousIndex];
            if (PreviousElement.Semantic == Element.Semantic && PreviousElement.SemanticIndex == Element.SemanticIndex)
            {
                return false;
            }
        }
    }

    for (uint32 Index : DataView.Indices)
    {
        if (Index >= DataView.VertexCount)
        {
            return false;
        }
    }

    return true;
}
