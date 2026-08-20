#include "Render/D3D11/D3D11Backend.h"

#include "Core/Assert.h"
#include "Render/Resource/MeshResources.h"

#include <d3d11.h>

#include <utility>

FD3D11Backend::FD3D11Backend() = default;

FD3D11Backend::~FD3D11Backend()
{
	Release();
}

void FD3D11Backend::Create(void* NativeWindowHandle)
{
	Release();
	checkf(NativeWindowHandle, "FD3D11Backend::Create에 전달된 NativeWindowHandle이 null이다.");

	Microsoft::WRL::ComPtr<IDXGISwapChain> SwapChain = Device.Create(NativeWindowHandle);
	Viewport.Create(Device.GetDevice(), std::move(SwapChain));
	Pipeline.Create(Device.GetDevice());
}

void FD3D11Backend::Release()
{
	Device.FlushAndUnbindTargets();
	BufferPool.Release();
	Pipeline.Release();
	Viewport.Release();
	Device.Release();
}

void FD3D11Backend::Prepare()
{
	ID3D11DeviceContext* DeviceContext = Device.GetContext();
	panic(DeviceContext);

	// ImGui가 이전 프레임 끝에서 Immediate Context 상태를 변경하므로 엔진 상태 캐시를 매 프레임 무효화한다.
	StateCache.Reset();
	Viewport.Prepare(DeviceContext);
	Pipeline.PrepareFrame(DeviceContext);
}

void FD3D11Backend::SwapBuffer()
{
	Viewport.Present();
}

FBufferHandle FD3D11Backend::CreateVertexBuffer(
	std::span<const uint8> Data, uint32 VertexCount, uint32 Stride)
{
	return BufferPool.CreateVertexBuffer(Device.GetDevice(), Data, VertexCount, Stride);
}

FBufferHandle FD3D11Backend::CreateIndexBuffer(std::span<const uint32> Indices)
{
	return BufferPool.CreateIndexBuffer(Device.GetDevice(), Indices);
}

void FD3D11Backend::DestroyBuffer(FBufferHandle& Handle)
{
	BufferPool.DestroyBuffer(Handle);
}

void FD3D11Backend::UpdateConstant(const FMatrix& WorldViewProjection)
{
	Pipeline.UpdateConstant(Device.GetContext(), WorldViewProjection);
}

void FD3D11Backend::DrawMeshBuffer(const FMeshBuffer& MeshBuffer)
{
	ID3D11Device* NativeDevice = Device.GetDevice();
	ID3D11DeviceContext* DeviceContext = Device.GetContext();

	check(NativeDevice);
	check(DeviceContext);
	checkf(MeshBuffer.IsValid(), "유효하지 않은 FMeshBuffer가 DrawMeshBuffer로 전달되었다.");

	const FVertexLayout& VertexLayout = MeshBuffer.GetLayout();
	ID3D11InputLayout* InputLayout = StateCache.InputLayout;
	if (StateCache.VertexLayout != &VertexLayout)
	{
		StateCache.VertexLayout = &VertexLayout;
		InputLayout = Pipeline.GetOrCreateInputLayout(NativeDevice, VertexLayout);
	}

	ID3D11Buffer* VertexBuffer = BufferPool.ResolveBuffer(MeshBuffer.GetVertexBuffer().GetHandle());
	
	check(InputLayout);
	checkf(VertexBuffer, "Vertex Buffer 핸들 해석 실패. Index={}, Generation={} (이미 파괴된 핸들)",
	       MeshBuffer.GetVertexBuffer().GetHandle().Index,
	       MeshBuffer.GetVertexBuffer().GetHandle().Generation);

	const UINT Stride = MeshBuffer.GetStride();
	if (StateCache.InputLayout != InputLayout)
	{
		DeviceContext->IASetInputLayout(InputLayout);
		StateCache.InputLayout = InputLayout;
	}
	if (StateCache.VertexBuffer != VertexBuffer || StateCache.VertexStride != Stride)
	{
		const UINT Offset = 0;
		DeviceContext->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);
		StateCache.VertexBuffer = VertexBuffer;
		StateCache.VertexStride = Stride;
	}

	if (MeshBuffer.GetIndexCount() > 0)
	{
		ID3D11Buffer* IndexBuffer = BufferPool.ResolveBuffer(MeshBuffer.GetIndexBuffer().GetHandle());
		checkf(IndexBuffer, "Index Buffer 핸들 해석 실패. Index={}, Generation={} (이미 파괴된 핸들)",
			MeshBuffer.GetVertexBuffer().GetHandle().Index,
			MeshBuffer.GetVertexBuffer().GetHandle().Generation);

		if (!StateCache.bIndexBufferKnown || StateCache.IndexBuffer != IndexBuffer)
		{
			DeviceContext->IASetIndexBuffer(IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
			StateCache.IndexBuffer = IndexBuffer;
			StateCache.bIndexBufferKnown = true;
		}
		DeviceContext->DrawIndexed(MeshBuffer.GetIndexCount(), 0, 0);
		return;
	}
	
	if (!StateCache.bIndexBufferKnown || StateCache.IndexBuffer)
	{
		DeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);
		StateCache.IndexBuffer = nullptr;
		StateCache.bIndexBufferKnown = true;
	}
	DeviceContext->Draw(MeshBuffer.GetVertexCount(), 0);
}

void FD3D11Backend::FD3D11StateCache::Reset()
{
	VertexLayout = nullptr;
	InputLayout = nullptr;
	VertexBuffer = nullptr;
	IndexBuffer = nullptr;
	VertexStride = 0;
	bIndexBufferKnown = false;
}

FRenderViewport FD3D11Backend::GetViewport() const
{
	return Viewport.GetViewport();
}

ID3D11Device* FD3D11Backend::GetNativeDevice() const
{
	return Device.GetDevice();
}

ID3D11DeviceContext* FD3D11Backend::GetNativeContext() const
{
	return Device.GetContext();
}
