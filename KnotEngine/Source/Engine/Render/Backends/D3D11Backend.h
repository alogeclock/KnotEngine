#pragma once

#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

class FD3D11Backend
{
public:
    FD3D11Backend();
    ~FD3D11Backend();

    FD3D11Backend(const FD3D11Backend&) = delete;
    FD3D11Backend& operator=(const FD3D11Backend&) = delete;
    FD3D11Backend(FD3D11Backend&&) = delete;
    FD3D11Backend& operator=(FD3D11Backend&&) = delete;

private:
    Microsoft::WRL::ComPtr<ID3D11Device> NativeDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> NativeContext;
    Microsoft::WRL::ComPtr<IDXGISwapChain> NativeSwapChain;
};
