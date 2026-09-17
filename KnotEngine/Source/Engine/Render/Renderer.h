#pragma once

#include "EngineAPI.h"

#include "Render/RHI/RenderTypes.h"
#include "Render/Resource/Mesh/Mesh.h"
#include "Render/Resource/State/PipelineStateCache.h"
#include "Render/Resource/State/SamplerStateCache.h"
#include "Render/Resource/Texture.h"
#include "Render/Shader/ShaderCompiler.h"
#include "Render/Shader/ShaderRegistry.h"

class FRenderGraph;
class IRenderContext;
class IRenderDevice;
class IShaderFormat;

class ENGINE_API URenderer
{
public:
	URenderer(IRenderDevice& InRenderDevice, IRenderContext& InRenderContext, IShaderFormat& InShaderFormat);
	~URenderer();

	URenderer(const URenderer&) = delete;
	URenderer& operator=(const URenderer&) = delete;
	URenderer(URenderer&&) = delete;
	URenderer& operator=(URenderer&&) = delete;

	void Create(void* NativeWindowHandle);
	void Release();
	void Resize(uint32 Width, uint32 Height);

	void Execute(FRenderGraph& RenderGraph);

	void BeginFrame();
	void EndFrame();

	void BeginRenderTarget(FTextureHandle ColorTarget, FTextureHandle DepthTarget, const FRenderViewport& Viewport);
	void EndRenderTarget();

	FShaderRegistry& GetShaderRegistry();
	FPipelineStateCache& GetPipelineStateCache();
	FSamplerStateCache& GetSamplerStateCache();

	FTextureHandle GetDefaultTexture() const { return DefaultTexture.GetHandle(); }
	FGeometryMesh& GetDebugBoundsMesh() { return DebugBoundsMesh; }

	IRenderDevice& GetRenderDevice() const;
	FCommandListHandle GetCommandList() const;
	FRenderViewport GetViewport() const;

private:
	IRenderDevice& RenderDevice;
	IRenderContext& RenderContext;

	FShaderCompiler ShaderCompiler;
	FShaderRegistry ShaderRegistry;
	FPipelineStateCache PipelineStateCache;
	FSamplerStateCache SamplerStateCache;

	FTexture DefaultTexture; // 1x1 White Texture
	FGeometryMesh DebugBoundsMesh; // TODO: 별도 DebugDraw() 파이프라인을 구현한다.

	FCommandListHandle CommandList;
};
