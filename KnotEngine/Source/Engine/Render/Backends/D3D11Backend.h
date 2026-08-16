#pragma once

#include "Render/RHI/RenderBackend.h"

#include <memory>
#include <span>

struct ID3D11Device;
struct ID3D11DeviceContext;

class FD3D11Backend final : public IRenderBackend
{
public:
    FD3D11Backend();
    ~FD3D11Backend() override;

    FD3D11Backend(const FD3D11Backend&) = delete;
    FD3D11Backend& operator=(const FD3D11Backend&) = delete;
    FD3D11Backend(FD3D11Backend&&) = delete;
    FD3D11Backend& operator=(FD3D11Backend&&) = delete;

    void Create(void* NativeWindowHandle) override;
    void Release() override;

    void Prepare() override;
    void SwapBuffer() override;

    FBufferHandle CreateVertexBuffer(std::span<const uint8> Data, uint32 VertexCount, uint32 Stride) override;
    FBufferHandle CreateIndexBuffer(std::span<const uint32> Indices) override;
    void DestroyBuffer(FBufferHandle& Handle) override;

    void UpdateConstant(const FMatrix& WorldViewProjection) override;
    void PrepareShader() override;
    void DrawMeshBuffer(const FMeshBuffer& MeshBuffer) override;

    FRenderViewport GetViewport() const override;

    ID3D11Device* GetNativeDevice() const;
    ID3D11DeviceContext* GetNativeContext() const;

private:
    struct FImpl;
    std::unique_ptr<FImpl> Impl;
};
