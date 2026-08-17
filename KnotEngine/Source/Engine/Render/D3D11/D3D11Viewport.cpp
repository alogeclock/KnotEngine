#include "Render/D3D11/D3D11Viewport.h"

#include "Core/Assert.h"
#include "Core/CoreTypes.h"

#include <d3d11.h>
#include <dxgi.h>

#include <utility>

void FD3D11Viewport::Create(
    ID3D11Device* Device, Microsoft::WRL::ComPtr<IDXGISwapChain>&& InSwapChain)
{
	Release();
	panic(Device);
	panic(InSwapChain);
	SwapChain = std::move(InSwapChain);

	DXGI_SWAP_CHAIN_DESC SwapChainDesc = {};
	HRESULT Result = SwapChain->GetDesc(&SwapChainDesc);
	panicf(SUCCEEDED(Result), "IDXGISwapChain::GetDesc 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));
	Viewport = {
		0.0f,
		0.0f,
		static_cast<float>(SwapChainDesc.BufferDesc.Width),
		static_cast<float>(SwapChainDesc.BufferDesc.Height),
		0.0f,
		1.0f,
	};

	Result = SwapChain->GetBuffer(0, IID_PPV_ARGS(FrameBuffer.ReleaseAndGetAddressOf()));
	panicf(SUCCEEDED(Result), "IDXGISwapChain::GetBuffer(0) 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));

	Result = Device->CreateRenderTargetView(FrameBuffer.Get(), nullptr, FrameBufferRTV.ReleaseAndGetAddressOf());
	panicf(SUCCEEDED(Result), "ID3D11Device::CreateRenderTargetView 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));

	D3D11_TEXTURE2D_DESC DepthDesc = {};
	DepthDesc.Width = static_cast<UINT>(Viewport.Width);
	DepthDesc.Height = static_cast<UINT>(Viewport.Height);
	DepthDesc.MipLevels = 1;
	DepthDesc.ArraySize = 1;
	DepthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	DepthDesc.SampleDesc.Count = 1;
	DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	Result = Device->CreateTexture2D(&DepthDesc, nullptr, DepthStencilBuffer.ReleaseAndGetAddressOf());
	panicf(SUCCEEDED(Result), "ID3D11Device::CreateTexture2D(DepthStencil) 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));

	D3D11_DEPTH_STENCIL_VIEW_DESC DepthViewDesc = {};
	DepthViewDesc.Format = DepthDesc.Format;
	DepthViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	Result = Device->CreateDepthStencilView(DepthStencilBuffer.Get(), &DepthViewDesc, DepthStencilView.ReleaseAndGetAddressOf());
	panicf(SUCCEEDED(Result), "ID3D11Device::CreateDepthStencilView 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));
}

void FD3D11Viewport::Release()
{
	DepthStencilView.Reset();
	DepthStencilBuffer.Reset();
	FrameBufferRTV.Reset();
	FrameBuffer.Reset();
	SwapChain.Reset();
	Viewport = {};
}

void FD3D11Viewport::Prepare(ID3D11DeviceContext* DeviceContext)
{
	panic(DeviceContext);
	panic(FrameBufferRTV);
	panic(DepthStencilView);

	DeviceContext->ClearRenderTargetView(FrameBufferRTV.Get(), ClearColor);
	DeviceContext->ClearDepthStencilView(DepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	D3D11_VIEWPORT NativeViewport = {
		Viewport.TopLeftX,
		Viewport.TopLeftY,
		Viewport.Width,
		Viewport.Height,
		Viewport.MinDepth,
		Viewport.MaxDepth,
	};
	ID3D11RenderTargetView* RenderTarget = FrameBufferRTV.Get();
	DeviceContext->RSSetViewports(1, &NativeViewport);
	DeviceContext->OMSetRenderTargets(1, &RenderTarget, DepthStencilView.Get());
}

void FD3D11Viewport::Present()
{
	panic(SwapChain);
	const HRESULT Result = SwapChain->Present(1, 0);
	panicf(SUCCEEDED(Result), "IDXGISwapChain::Present 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));
}
