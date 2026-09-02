#include "Render/D3D11/D3D11RenderContext.h"

#include "Core/Assert.h"
#include "Render/D3D11/D3D11RenderDevice.h"

#include <d3d11.h>
#include <dxgi.h>

// Swap Chain과 Native Device가 동일한 D3D11 Device를 사용하도록 소유 Device를 주입받는다.
FD3D11RenderContext::FD3D11RenderContext(FD3D11RenderDevice& InRenderDevice)
	: RenderDevice(InRenderDevice)
{
}

// Context가 소유한 화면 출력 자원을 해제한다.
FD3D11RenderContext::~FD3D11RenderContext()
{
	Release();
}

// Native Window에 연결된 Flip Model Swap Chain과 프레임 출력 대상을 생성한다.
void FD3D11RenderContext::Create(void* NativeWindowHandle)
{
	Release();
	checkf(NativeWindowHandle, "FD3D11RenderContext::Create에 전달된 NativeWindowHandle이 null이다.");
	panic(RenderDevice.GetNativeDevice());

	// D3D11 Device가 사용 중인 Adapter와 Factory를 따라가 동일한 GPU에 Swap Chain을 생성한다.
	Microsoft::WRL::ComPtr<IDXGIDevice> DxgiDevice;
	HRESULT Result = RenderDevice.GetNativeDevice()->QueryInterface(IID_PPV_ARGS(DxgiDevice.GetAddressOf()));
	panicf(SUCCEEDED(Result) && DxgiDevice, "ID3D11Device에서 IDXGIDevice 조회 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));

	Microsoft::WRL::ComPtr<IDXGIAdapter> Adapter;
	Result = DxgiDevice->GetAdapter(Adapter.GetAddressOf());
	panicf(SUCCEEDED(Result) && Adapter, "IDXGIDevice::GetAdapter 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));

	Microsoft::WRL::ComPtr<IDXGIFactory> Factory;
	Result = Adapter->GetParent(IID_PPV_ARGS(Factory.GetAddressOf()));
	panicf(SUCCEEDED(Result) && Factory, "IDXGIAdapter::GetParent 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));

	DXGI_SWAP_CHAIN_DESC Desc = {};
	Desc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	Desc.SampleDesc.Count = 1;
	Desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	Desc.BufferCount = 2;
	Desc.OutputWindow = static_cast<HWND>(NativeWindowHandle);
	Desc.Windowed = TRUE;
	Desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	Result = Factory->CreateSwapChain(RenderDevice.GetNativeDevice(), &Desc, SwapChain.GetAddressOf());
	panicf(SUCCEEDED(Result) && SwapChain, "IDXGIFactory::CreateSwapChain 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));

	CreateFrameTargets();
}

// Device Context의 출력 대상 참조를 끊고 Swap Chain 종속 자원을 해제한다.
void FD3D11RenderContext::Release()
{
	// Context가 View를 참조한 상태에서 Back Buffer를 해제하지 않도록 출력 대상을 먼저 Unbind한다.
	if (RenderDevice.GetNativeContext())
	{
		RenderDevice.GetNativeContext()->OMSetRenderTargets(0, nullptr, nullptr);
	}
	ReleaseFrameTargets();
	SwapChain.Reset();
	Viewport = {};
}

// 기존 Back Buffer 종속 자원을 해제하고 새 창 크기에 맞춰 다시 생성한다.
// 크기가 0인 최소화 상태는 백엔드가 유효한 출력 크기를 받을 때까지 보류한다.
void FD3D11RenderContext::Resize(uint32 Width, uint32 Height)
{
	// 최소화 중 전달되는 0 크기로는 DXGI Buffer를 재생성하지 않는다.
	if (!SwapChain || Width == 0 || Height == 0)
	{
		return;
	}

	RenderDevice.GetNativeContext()->OMSetRenderTargets(0, nullptr, nullptr);
	ReleaseFrameTargets();
	const HRESULT Result = SwapChain->ResizeBuffers(0, Width, Height, DXGI_FORMAT_UNKNOWN, 0);
	panicf(SUCCEEDED(Result), "IDXGISwapChain::ResizeBuffers 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));
	CreateFrameTargets();
}

// Back Buffer와 Depth Buffer를 초기화하고 현재 프레임의 출력 대상으로 설정한다.
void FD3D11RenderContext::BeginFrame(FCommandListHandle CommandList)
{
	check(CommandList.IsValid());
	ID3D11DeviceContext* Context = RenderDevice.GetNativeContext();
	panic(Context);
	panic(FrameBufferRTV);
	panic(DepthStencilView);

	Context->ClearRenderTargetView(FrameBufferRTV.Get(), ClearColor);
	Context->ClearDepthStencilView(DepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	D3D11_VIEWPORT NativeViewport = {
		Viewport.TopLeftX, Viewport.TopLeftY, Viewport.Width, Viewport.Height,
		Viewport.MinDepth, Viewport.MaxDepth
	};
	ID3D11RenderTargetView* RenderTarget = FrameBufferRTV.Get();
	Context->RSSetViewports(1, &NativeViewport);
	Context->OMSetRenderTargets(1, &RenderTarget, DepthStencilView.Get());
}

// D3D11 Back Buffer에는 명시적인 Present 상태 전환이 없으므로 기록 구간만 검증한다.
void FD3D11RenderContext::EndFrame(FCommandListHandle CommandList)
{
	RenderDevice.ValidateCommandList(CommandList);
}

// 완성된 Back Buffer를 DXGI Swap Chain을 통해 화면에 표시한다.
void FD3D11RenderContext::Present()
{
	panic(SwapChain);
	const HRESULT Result = SwapChain->Present(0, 0);
	panicf(SUCCEEDED(Result), "IDXGISwapChain::Present 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));
}

// 현재 Back Buffer의 Render Target View와 같은 크기의 Depth Target을 생성한다.
void FD3D11RenderContext::CreateFrameTargets()
{
	panic(SwapChain);
	panic(RenderDevice.GetNativeDevice());

	DXGI_SWAP_CHAIN_DESC SwapChainDesc = {};
	HRESULT Result = SwapChain->GetDesc(&SwapChainDesc);
	panicf(SUCCEEDED(Result), "IDXGISwapChain::GetDesc 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));
	Viewport = { 0.0f, 0.0f, static_cast<float>(SwapChainDesc.BufferDesc.Width), static_cast<float>(SwapChainDesc.BufferDesc.Height), 0.0f, 1.0f };

	// Back Buffer와 동일한 크기의 Depth Target을 한 묶음으로 생성한다.
	Result = SwapChain->GetBuffer(0, IID_PPV_ARGS(FrameBuffer.GetAddressOf()));
	panicf(SUCCEEDED(Result), "IDXGISwapChain::GetBuffer 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));
	Result = RenderDevice.GetNativeDevice()->CreateRenderTargetView(FrameBuffer.Get(), nullptr, FrameBufferRTV.GetAddressOf());
	panicf(SUCCEEDED(Result), "ID3D11Device::CreateRenderTargetView 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));

	D3D11_TEXTURE2D_DESC DepthDesc = {};
	DepthDesc.Width = static_cast<UINT>(Viewport.Width);
	DepthDesc.Height = static_cast<UINT>(Viewport.Height);
	DepthDesc.MipLevels = 1;
	DepthDesc.ArraySize = 1;
	DepthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	DepthDesc.SampleDesc.Count = 1;
	DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	Result = RenderDevice.GetNativeDevice()->CreateTexture2D(&DepthDesc, nullptr, DepthStencilBuffer.GetAddressOf());
	panicf(SUCCEEDED(Result), "ID3D11Device::CreateTexture2D(Depth) 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));
	Result = RenderDevice.GetNativeDevice()->CreateDepthStencilView(DepthStencilBuffer.Get(), nullptr, DepthStencilView.GetAddressOf());
	panicf(SUCCEEDED(Result), "ID3D11Device::CreateDepthStencilView 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));
}

// Swap Chain의 크기와 수명에 종속된 Color 및 Depth Target 자원을 해제한다.
void FD3D11RenderContext::ReleaseFrameTargets()
{
	DepthStencilView.Reset();
	DepthStencilBuffer.Reset();
	FrameBufferRTV.Reset();
	FrameBuffer.Reset();
}
