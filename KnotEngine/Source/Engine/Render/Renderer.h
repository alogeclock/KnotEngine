#pragma once

#include "EngineAPI.h"

#include "Asset/Asset/AssetId.h"
#include "Render/RHI/RenderTypes.h"
#include "Render/DebugDraw/DebugDraw.h"
#include "Render/Resource/MaterialResource.h"
#include "Render/Resource/Mesh/StaticMeshResource.h"
#include "Render/Resource/State/PipelineStateCache.h"
#include "Render/Resource/State/SamplerStateCache.h"
#include "Render/Resource/TextureResource.h"
#include "Render/Shader/ShaderCompiler.h"
#include "Render/Shader/ShaderRegistry.h"

#include <memory>

class FRenderGraph;
class IRenderContext;
class IRenderDevice;
class IShaderFormat;
class UMaterialInterface;
class UTexture2D;
class FStaticMesh;

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
	void ReleaseAssetReferences();
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
	FDebugDraw& GetDebugDraw() { return DebugDraw; }
	FMaterialResource& GetOrCreateMaterialResource(const UMaterialInterface& MaterialInterface);
	FStaticMeshResource& GetOrCreateStaticMeshResource(const FAssetId& AssetId, const FStaticMesh& StaticMesh, uint64 Revision);
	FTextureResource& GetOrCreateTextureResource(const UTexture2D& Texture);
	const FMaterialResource& GetDefaultMaterialResource() const { return DefaultMaterialResource; }
	const FTextureResource& GetDefaultTextureResource() const { return DefaultTextureResource; }

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

	FMaterialResource DefaultMaterialResource;
	FTextureResource DefaultTextureResource; // 1x1 White Texture
	FDebugDraw DebugDraw;

	TMap<FAssetId, std::unique_ptr<FMaterialResource>, FAssetIdHash> MaterialResources;
	TMap<FAssetId, std::unique_ptr<FStaticMeshResource>, FAssetIdHash> StaticMeshResources;
	TMap<FAssetId, std::unique_ptr<FTextureResource>, FAssetIdHash> TextureResources;

	FCommandListHandle CommandList;
};
