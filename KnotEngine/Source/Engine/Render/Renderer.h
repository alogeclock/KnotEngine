#pragma once

#include "Render/RHI/RenderTypes.h"

#include <span>

class FIndexBuffer;
class FMeshBuffer;
class FVertexBuffer;
class IRenderContext;
class IRenderDevice;
struct FMatrix;

class URenderer
{
public:
	URenderer(IRenderDevice& InRenderDevice, IRenderContext& InRenderContext);
	~URenderer();

	URenderer(const URenderer&) = delete;
	URenderer& operator=(const URenderer&) = delete;
	URenderer(URenderer&&) = delete;
	URenderer& operator=(URenderer&&) = delete;

	void Create(void* NativeWindowHandle);
	void Release();

	void BeginFrame();
	void EndFrame();

	void UpdateConstant(const FMatrix& WorldViewProjection);
	void DrawMeshBuffer(const FMeshBuffer& MeshBuffer);

	FRenderViewport GetViewport() const;

private:
	friend class FMeshBuffer;

	bool CreateVertexBuffer(FVertexBuffer& OutVertexBuffer, std::span<const uint8> Data, uint32 VertexCount, uint32 Stride);
	bool CreateIndexBuffer(FIndexBuffer& OutIndexBuffer, std::span<const uint32> Indices);

	IRenderDevice& RenderDevice;
	IRenderContext& RenderContext;
	FShaderHandle VertexShader;
	FShaderHandle PixelShader;
	FGraphicsPipelineHandle GraphicsPipeline;
	FBufferHandle FrameConstantBuffer;
	FCommandListHandle CommandList;
};
