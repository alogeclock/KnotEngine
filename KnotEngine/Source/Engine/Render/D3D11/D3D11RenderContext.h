#pragma once

#include "Render/RHI/RenderContext.h"

#include <wrl/client.h>

struct ID3D11DepthStencilView;
struct ID3D11RenderTargetView;
struct ID3D11Texture2D;
struct IDXGISwapChain;

class FD3D11RenderDevice;

// D3D11 Swap Chain과 화면용 Color/Depth Target의 생성 및 프레임 출력을 담당한다.
class FD3D11RenderContext final : public IRenderContext
{
public:
	explicit FD3D11RenderContext(FD3D11RenderDevice& InRenderDevice);
	~FD3D11RenderContext() override;

	void Create(void* NativeWindowHandle) override;
	void Release() override;

	void Resize(uint32 Width, uint32 Height) override;
	void BeginFrame(FCommandListHandle CommandList) override;
	void EndFrame(FCommandListHandle CommandList) override;
	void Present() override;

	FRenderViewport GetViewport() const override { return Viewport; }

private:
	// Swap Chain 생성 및 Resize 이후 Back Buffer에 종속된 View들을 함께 재생성한다.
	void CreateFrameTargets();
	void ReleaseFrameTargets();

	FD3D11RenderDevice& RenderDevice;
	Microsoft::WRL::ComPtr<IDXGISwapChain> SwapChain;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> FrameBuffer;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> FrameBufferRTV;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> DepthStencilBuffer;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> DepthStencilView;
	FRenderViewport Viewport;
	float ClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f };
};
