#pragma once

#include "Render/D3D11/D3D11BufferPool.h"
#include "Render/D3D11/D3D11Device.h"
#include "Render/D3D11/D3D11Pipeline.h"
#include "Render/D3D11/D3D11Viewport.h"
#include "Render/RHI/RenderBackend.h"

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
	void DrawMeshBuffer(const FMeshBuffer& MeshBuffer) override;

	FRenderViewport GetViewport() const override;

	ID3D11Device* GetNativeDevice() const;
	ID3D11DeviceContext* GetNativeContext() const;

private:
	struct FD3D11StateCache
	{
		const FVertexLayout* VertexLayout = nullptr;
		ID3D11InputLayout* InputLayout = nullptr;
		ID3D11Buffer* VertexBuffer = nullptr;
		ID3D11Buffer* IndexBuffer = nullptr;
		uint32 VertexStride = 0;
		bool bIndexBufferKnown = false;

		void Reset();
	};

	FD3D11StateCache StateCache;

	FD3D11Device Device;
	FD3D11Viewport Viewport;
	FD3D11BufferPool BufferPool;
	FD3D11Pipeline Pipeline;
};
