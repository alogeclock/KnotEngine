#pragma once

#include "Render/RHI/RenderBackend.h"

#include <memory>
#include <span>

class FD3D12Backend final : public IRenderBackend
{
public:
	FD3D12Backend();
	~FD3D12Backend() override;

	FD3D12Backend(const FD3D12Backend&) = delete;
	FD3D12Backend& operator=(const FD3D12Backend&) = delete;
	FD3D12Backend(FD3D12Backend&&) = delete;
	FD3D12Backend& operator=(FD3D12Backend&&) = delete;

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

private:
	struct FImpl;
	std::unique_ptr<FImpl> Impl;
};
