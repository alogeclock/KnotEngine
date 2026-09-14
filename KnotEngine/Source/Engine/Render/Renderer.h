#pragma once

#include "EngineAPI.h"

#include "Render/RHI/RenderTypes.h"
#include "Render/Resource/PipelineStateCache.h"
#include "Render/Resource/ShaderRegistry.h"

class FRenderGraph;
class IRenderContext;
class IRenderDevice;

class ENGINE_API URenderer
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
	void Resize(uint32 Width, uint32 Height);

	void BeginFrame();
	void EndFrame();
	void Execute(FRenderGraph& RenderGraph);

	void BeginRenderTarget(FTextureHandle ColorTarget, FTextureHandle DepthTarget, const FRenderViewport& Viewport);
	void EndRenderTarget();

	FShaderRegistry& GetShaderRegistry();
	FPipelineStateCache& GetPipelineStateCache();

	IRenderDevice& GetRenderDevice() const;
	FCommandListHandle GetCommandList() const;
	FRenderViewport GetViewport() const;

private:
	IRenderDevice& RenderDevice;
	IRenderContext& RenderContext;

	FShaderRegistry ShaderRegistry;
	FPipelineStateCache PipelineStateCache;

	FCommandListHandle CommandList;
};
