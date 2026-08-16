#pragma once

#include "Render/RHI/RenderTypes.h"

#include <wrl/client.h>

struct ID3D11DepthStencilView;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11RenderTargetView;
struct ID3D11Texture2D;
struct IDXGISwapChain;

class FD3D11Viewport final
{
public:
	void Create(ID3D11Device* Device, Microsoft::WRL::ComPtr<IDXGISwapChain>&& SwapChain);
	void Release();

	void Prepare(ID3D11DeviceContext* DeviceContext);
	void Present();

	FRenderViewport GetViewport() const { return Viewport; }

private:
	Microsoft::WRL::ComPtr<IDXGISwapChain> SwapChain;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> FrameBuffer;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> FrameBufferRTV;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> DepthStencilBuffer;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> DepthStencilView;
	FRenderViewport Viewport;
	float ClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f };
};
