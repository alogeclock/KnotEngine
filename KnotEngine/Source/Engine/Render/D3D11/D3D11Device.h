#pragma once

#include <wrl/client.h>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct IDXGISwapChain;

class FD3D11Device final
{
public:
	Microsoft::WRL::ComPtr<IDXGISwapChain> Create(void* NativeWindowHandle);
	void Release();

	void FlushAndUnbindTargets();

	ID3D11Device* GetDevice() const { return Device.Get(); }
	ID3D11DeviceContext* GetContext() const { return DeviceContext.Get(); }

private:
	Microsoft::WRL::ComPtr<ID3D11Device> Device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> DeviceContext;
};
