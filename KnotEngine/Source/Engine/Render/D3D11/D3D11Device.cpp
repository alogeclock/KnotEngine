#include "Render/D3D11/D3D11Device.h"

#include "Core/Assert.h"
#include "Core/CoreTypes.h"

#include <d3d11.h>
#include <dxgi.h>

#pragma comment(lib, "d3d11.lib")

Microsoft::WRL::ComPtr<IDXGISwapChain> FD3D11Device::Create(void* NativeWindowHandle)
{
	Release();
	checkf(NativeWindowHandle, "FD3D11Device::Create에 전달된 NativeWindowHandle이 null이다.");

	const D3D_FEATURE_LEVEL FeatureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
	DXGI_SWAP_CHAIN_DESC SwapChainDesc = {};
	SwapChainDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	SwapChainDesc.SampleDesc.Count = 1;
	SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	SwapChainDesc.BufferCount = 2;
	SwapChainDesc.OutputWindow = static_cast<HWND>(NativeWindowHandle);
	SwapChainDesc.Windowed = TRUE;
	SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	Microsoft::WRL::ComPtr<IDXGISwapChain> SwapChain;
	UINT CreateFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG;
	HRESULT Result = D3D11CreateDeviceAndSwapChain(
	    nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, CreateFlags,
	    FeatureLevels, ARRAYSIZE(FeatureLevels), D3D11_SDK_VERSION,
	    &SwapChainDesc, SwapChain.GetAddressOf(), Device.GetAddressOf(), nullptr,
	    DeviceContext.GetAddressOf());
	if (FAILED(Result))
	{
		CreateFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
		Result = D3D11CreateDeviceAndSwapChain(
		    nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, CreateFlags,
		    FeatureLevels, ARRAYSIZE(FeatureLevels), D3D11_SDK_VERSION,
		    &SwapChainDesc, SwapChain.ReleaseAndGetAddressOf(),
		    Device.ReleaseAndGetAddressOf(), nullptr,
		    DeviceContext.ReleaseAndGetAddressOf());
	}

	panicf(SUCCEEDED(Result) && Device && DeviceContext && SwapChain,
	       "D3D11CreateDeviceAndSwapChain 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));
	return SwapChain;
}

void FD3D11Device::Release()
{
	DeviceContext.Reset();
	Device.Reset();
}

void FD3D11Device::FlushAndUnbindTargets()
{
	if (!DeviceContext)
	{
		return;
	}

	DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
	DeviceContext->Flush();
}
