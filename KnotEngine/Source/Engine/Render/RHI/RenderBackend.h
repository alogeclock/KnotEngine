#pragma once

#include "Render/RHI/RenderTypes.h"

#include <span>

class FMeshBuffer;
struct FMatrix;

class IRenderBackend
{
public:
	IRenderBackend() = default;
	virtual ~IRenderBackend() = default;

	IRenderBackend(const IRenderBackend&) = delete;
	IRenderBackend& operator=(const IRenderBackend&) = delete;
	IRenderBackend(IRenderBackend&&) = delete;
	IRenderBackend& operator=(IRenderBackend&&) = delete;

	virtual void Create(void* NativeWindowHandle) = 0;
	virtual void Release() = 0;

	virtual void Prepare() = 0;
	virtual void SwapBuffer() = 0;

	virtual FBufferHandle CreateVertexBuffer(std::span<const uint8> Data, uint32 VertexCount, uint32 Stride) = 0;
	virtual FBufferHandle CreateIndexBuffer(std::span<const uint32> Indices) = 0;
	virtual void DestroyBuffer(FBufferHandle& Handle) = 0;

	virtual void UpdateConstant(const FMatrix& WorldViewProjection) = 0;
	virtual void PrepareShader() = 0;
	virtual void DrawMeshBuffer(const FMeshBuffer& MeshBuffer) = 0;

	virtual FRenderViewport GetViewport() const = 0;
};
