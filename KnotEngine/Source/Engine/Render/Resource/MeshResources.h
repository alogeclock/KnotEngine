#pragma once

#include "Render/Resource/Buffer.h"
#include "Render/Resource/RenderResource.h"
#include "Render/Resource/VertexLayouts.h"

#include <span>

class URenderer;

// GPU 생성 호출 동안만 유효한 비소유 업로드 뷰.
struct FMeshDataView
{
    std::span<const uint8> VertexBytes;
    std::span<const uint32> Indices;
    const FVertexLayout* Layout = nullptr;
    uint32 VertexCount = 0;
};

// 하나의 Draw/DrawIndexed 호출에 필요한 GPU Mesh Buffer 묶음.
// FStaticMesh, FSkeletalMesh 등 상위 계층에서 FMeshBuffer를 소유한다.
class FMeshBuffer final : public FRenderResource
{
public:
    FMeshBuffer() = default;
    ~FMeshBuffer() override;

    bool Initialize(URenderer& Renderer, const FMeshDataView& InDataView);
    bool IsValid() const;

    const FVertexBuffer& GetVertexBuffer() const { return VertexBuffer; }
    const FIndexBuffer& GetIndexBuffer() const { return IndexBuffer; }
    const FVertexLayout& GetLayout() const { return VertexLayout; }

    uint32 GetVertexCount() const { return VertexBuffer.GetVertexCount(); }
    uint32 GetIndexCount() const { return IndexBuffer.GetIndexCount(); }
    uint32 GetStride() const { return VertexLayout.Stride; }

protected:
    void OnRelease() override;

private:
    static bool Validate(const FMeshDataView& DataView);

private:
    FVertexBuffer VertexBuffer;
    FIndexBuffer IndexBuffer;
    FVertexLayout VertexLayout;
};
