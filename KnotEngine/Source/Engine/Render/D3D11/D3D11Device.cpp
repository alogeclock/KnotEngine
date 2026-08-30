#include "Render/D3D11/D3D11Device.h"

#include "Core/Assert.h"
#include "Core/CoreTypes.h"

#include <d3d11.h>

#pragma comment(lib, "d3d11.lib")

FD3D11Device::FD3D11Device() = default;
FD3D11Device::~FD3D11Device() = default;

void FD3D11Device::Create()
{
	Release();

	const D3D_FEATURE_LEVEL FeatureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
	UINT CreateFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#if !defined(KNOT_BUILD_SHIPPING)
	CreateFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	HRESULT Result = D3D11CreateDevice(
		nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, CreateFlags,
		FeatureLevels, ARRAYSIZE(FeatureLevels), D3D11_SDK_VERSION,
		Device.GetAddressOf(), nullptr, DeviceContext.GetAddressOf());

#if !defined(KNOT_BUILD_SHIPPING)
	if (FAILED(Result) && (CreateFlags & D3D11_CREATE_DEVICE_DEBUG) != 0)
	{
		CreateFlags &= ~D3D11_CREATE_DEVICE_DEBUG;
		Result = D3D11CreateDevice(
			nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, CreateFlags,
			FeatureLevels, ARRAYSIZE(FeatureLevels), D3D11_SDK_VERSION,
			Device.ReleaseAndGetAddressOf(), nullptr, DeviceContext.ReleaseAndGetAddressOf());
	}
#endif

	panicf(SUCCEEDED(Result) && Device && DeviceContext,
		"D3D11CreateDevice 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));
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
