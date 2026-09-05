#pragma once

#include "RendererAPI.h"

#include "Render/D3D11/D3D11BufferPool.h"
#include "Render/D3D11/D3D11Device.h"
#include "Render/RHI/RenderDevice.h"

#include <vector>
#include <wrl/client.h>

struct ID3D10Blob;
struct ID3D11DepthStencilState;
struct ID3D11BlendState;
struct ID3D11Buffer;
struct ID3D11InputLayout;
struct ID3D11PixelShader;
struct ID3D11RasterizerState;
struct ID3D11Texture2D;
struct ID3D11VertexShader;

// API 중립 RHI 요청을 D3D11 자원 생성과 Immediate Context 명령으로 변환하는 Render Device 구현체다.
class RENDERER_API FD3D11RenderDevice final : public IRenderDevice
{
public:
	FD3D11RenderDevice();
	~FD3D11RenderDevice() override;

	void Create() override;
	void Release() override;

	FBufferHandle CreateBuffer(const FBufferDesc& Desc, std::span<const uint8> InitialData) override;
	void UpdateBuffer(FBufferHandle Handle, std::span<const uint8> Data) override;
	void DestroyBuffer(FBufferHandle& Handle) override;

	FTextureHandle CreateTexture(const FTextureDesc& Desc, std::span<const uint8> InitialData) override;
	void DestroyTexture(FTextureHandle& Handle) override;

	FShaderHandle CreateShader(const FShaderDesc& Desc) override;
	void DestroyShader(FShaderHandle& Handle) override;

	FGraphicsPipelineHandle CreateGraphicsPipeline(const FGraphicsPipelineDesc& Desc) override;
	void DestroyGraphicsPipeline(FGraphicsPipelineHandle& Handle) override;

	FCommandListHandle BeginCommandList() override;
	void EndCommandList(FCommandListHandle CommandList) override;
	void Submit(FCommandListHandle& CommandList) override;

	void SetGraphicsPipeline(FCommandListHandle CommandList, FGraphicsPipelineHandle Pipeline) override;
	void SetVertexBuffer(FCommandListHandle CommandList, FBufferHandle Buffer, uint32 Stride, uint32 Offset) override;
	void SetIndexBuffer(FCommandListHandle CommandList, FBufferHandle Buffer, EIndexFormat Format, uint32 Offset) override;
	void SetConstantData(FCommandListHandle CommandList, EShaderStage Stage, uint32 Slot, std::span<const uint8> Data) override;

	void Draw(FCommandListHandle CommandList, uint32 VertexCount, uint32 FirstVertex) override;
	void DrawIndexed(FCommandListHandle CommandList, uint32 IndexCount, uint32 FirstIndex, int32 VertexOffset) override;

	ID3D11Device* GetNativeDevice() const { return NativeDevice.GetDevice(); }
	ID3D11DeviceContext* GetNativeContext() const { return NativeDevice.GetContext(); }

private:
	friend class FD3D11RenderContext;

	// 외부에는 네이티브 포인터 대신 Index와 Generation으로 구성된 Handle만 노출한다.
	struct FTextureSlot
	{
		Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture;
		uint32 Generation = 1;
	};

	struct FShaderSlot
	{
		EShaderStage Stage = EShaderStage::Vertex;
		Microsoft::WRL::ComPtr<ID3D11VertexShader> VertexShader;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> PixelShader;
		Microsoft::WRL::ComPtr<ID3D10Blob> Bytecode;
		uint32 Generation = 1;
	};

	struct FPipelineSlot
	{
		Microsoft::WRL::ComPtr<ID3D11BlendState> BlendState;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilState> DepthStencilState;
		Microsoft::WRL::ComPtr<ID3D11RasterizerState> RasterizerState;
		Microsoft::WRL::ComPtr<ID3D11InputLayout> InputLayout;
		FShaderHandle VertexShader;
		FShaderHandle PixelShader;
		EPrimitiveTopology PrimitiveTopology = EPrimitiveTopology::TriangleList;
		uint32 Generation = 1;
		bool bValid = false;
	};

	struct FConstantBufferBinding
	{
		Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer;
		EShaderStage Stage = EShaderStage::Vertex;
		uint32 Slot = 0;
		uint32 Size = 0;
	};

	// D3D11에서는 하나의 Immediate Context를 세대가 있는 논리 Command List로 감싼다.
	void ValidateCommandList(FCommandListHandle CommandList) const;
	FShaderSlot* ResolveShader(FShaderHandle Handle);
	const FShaderSlot* ResolveShader(FShaderHandle Handle) const;
	FPipelineSlot* ResolvePipeline(FGraphicsPipelineHandle Handle);
	static void AdvanceGeneration(uint32& Generation);

	// Device가 모든 GPU 자원을 소유하고 Render Context는 Swap Chain 자원만 소유한다.
	FD3D11Device NativeDevice;
	FD3D11BufferPool BufferPool;
	std::vector<FTextureSlot> TextureSlots;
	std::vector<FShaderSlot> ShaderSlots;
	std::vector<FPipelineSlot> PipelineSlots;
	std::vector<FConstantBufferBinding> ConstantBufferBindings;
	uint32 CommandListGeneration = 1;
	bool bCommandListOpen = false;
};
