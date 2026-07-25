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

    ID3D11Device* Device = Renderer.GetDevice();
    if (!VertexBuffer.Initialize(Device, InDataView.VertexBytes.data(), InDataView.VertexCount, UploadLayout.Stride))
    {
        return false;
    }

    if (!InDataView.Indices.empty() &&
        !IndexBuffer.Initialize(Device, InDataView.Indices.data(), static_cast<uint32>(InDataView.Indices.size())))
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
