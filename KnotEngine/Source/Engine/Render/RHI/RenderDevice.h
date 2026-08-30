#pragma once

#include "Render/RHI/RenderTypes.h"

#include <span>

// GPU 자원 생성과 렌더링 명령 기록을 제공하는 API 중립 계층이다.
// D3D11은 Immediate Context로 명령을 실행하고, 명시적 API는 실제 Command List에 기록할 수 있다.
class IRenderDevice
{
public:
	IRenderDevice() = default;
	virtual ~IRenderDevice() = default;

	IRenderDevice(const IRenderDevice&) = delete;
	IRenderDevice& operator=(const IRenderDevice&) = delete;
	IRenderDevice(IRenderDevice&&) = delete;
	IRenderDevice& operator=(IRenderDevice&&) = delete;

	virtual void Create() = 0;
	virtual void Release() = 0;

	// 자원 생성 함수는 네이티브 자원까지 검증한 뒤 유효한 Handle만 반환한다.
	virtual FBufferHandle CreateBuffer(const FBufferDesc& Desc, std::span<const uint8> InitialData = {}) = 0;
	virtual void UpdateBuffer(FBufferHandle Handle, std::span<const uint8> Data) = 0;
	virtual void DestroyBuffer(FBufferHandle& Handle) = 0;

	virtual FTextureHandle CreateTexture(const FTextureDesc& Desc, std::span<const uint8> InitialData = {}) = 0;
	virtual void DestroyTexture(FTextureHandle& Handle) = 0;

	virtual FShaderHandle CreateShader(const FShaderDesc& Desc) = 0;
	virtual void DestroyShader(FShaderHandle& Handle) = 0;

	virtual FGraphicsPipelineHandle CreateGraphicsPipeline(const FGraphicsPipelineDesc& Desc) = 0;
	virtual void DestroyGraphicsPipeline(FGraphicsPipelineHandle& Handle) = 0;

	// Command List는 한 번 Begin한 뒤 End와 Submit을 순서대로 호출해야 한다.
	virtual FCommandListHandle BeginCommandList() = 0;
	virtual void EndCommandList(FCommandListHandle CommandList) = 0;

	virtual void Submit(FCommandListHandle& CommandList) = 0;

	// 아래 함수는 열린 Command List에 Graphics 상태 및 Draw 명령을 기록한다.
	virtual void SetGraphicsPipeline(FCommandListHandle CommandList, FGraphicsPipelineHandle Pipeline) = 0;
	virtual void SetVertexBuffer(FCommandListHandle CommandList, FBufferHandle Buffer, uint32 Stride, uint32 Offset = 0) = 0;
	virtual void SetIndexBuffer(FCommandListHandle CommandList, FBufferHandle Buffer, EIndexFormat Format, uint32 Offset = 0) = 0;
	virtual void SetConstantBuffer(FCommandListHandle CommandList, EShaderStage Stage, uint32 Slot, FBufferHandle Buffer) = 0;

	virtual void Draw(FCommandListHandle CommandList, uint32 VertexCount, uint32 FirstVertex = 0) = 0;
	virtual void DrawIndexed(FCommandListHandle CommandList, uint32 IndexCount, uint32 FirstIndex = 0, int32 VertexOffset = 0) = 0;
};
