#pragma once

#include "RendererAPI.h"

#include "Render/ImGui/ImGuiRenderBackend.h"

#include <wrl/client.h>

class FD3D11RenderDevice;
struct ID3D11BlendState;
struct ID3D11Buffer;
struct ID3D11DepthStencilState;
struct ID3D11InputLayout;
struct ID3D11PixelShader;
struct ID3D11RasterizerState;
struct ID3D11SamplerState;
struct ID3D11VertexShader;

class RENDERER_API FD3D11ImGuiBackend final : public IImGuiRenderBackend
{
public:
	explicit FD3D11ImGuiBackend(FD3D11RenderDevice& InRenderDevice);
	~FD3D11ImGuiBackend() override;

	ImTextureID Startup(std::span<const uint8> FontPixels, uint32 FontWidth, uint32 FontHeight) override;
	void Render(FCommandListHandle CommandList, const FImGuiDrawDataCopy& DrawData) override;
	ImTextureID GetImGuiTextureID(FTextureHandle Texture) const override;
	void Shutdown() override;

private:
	bool EnsureBuffers(SIZE_T VertexCount, SIZE_T IndexCount);

	void SetupRenderState(const FImGuiDrawDataCopy& DrawData);

	FD3D11RenderDevice& RenderDevice;
	FTextureHandle FontTexture;

	Microsoft::WRL::ComPtr<ID3D11Buffer> VertexBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> IndexBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> VertexConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11VertexShader> VertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> PixelShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> InputLayout;
	Microsoft::WRL::ComPtr<ID3D11SamplerState> TextureSampler;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> RasterizerState;
	Microsoft::WRL::ComPtr<ID3D11BlendState> BlendState;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> DepthStencilState;

	SIZE_T VertexBufferSize = 0;
	SIZE_T IndexBufferSize = 0;
};
