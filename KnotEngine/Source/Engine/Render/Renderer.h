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

struct FMaterialResourceCommand;
struct FStaticMeshInstance;
struct FStaticMeshResourceCommand;
struct FTextureResourceCommand;

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
	
	void UpdateMaterialResource(const FMaterialResourceCommand& Command);
	void UpdateStaticMeshResource(const FStaticMeshResourceCommand& Command);
	void UpdateTextureResource(const FTextureResourceCommand& Command);

	FMaterialResource* FindMaterialResource(const FAssetId& AssetId) const;
	FStaticMeshResource* FindStaticMeshResource(const FAssetId& AssetId) const;
	FTextureResource* FindTextureResource(const FAssetId& AssetId) const;

	const FMaterialResource& GetDefaultMaterialResource() const { return DefaultMaterialResource; }
	const FTextureResource& GetDefaultTextureResource() const { return DefaultTextureResource; }

	IRenderDevice& GetRenderDevice() const;
	FCommandListHandle GetCommandList() const;
	FRenderViewport GetViewport() const;
	FBufferHandle UploadStaticMeshInstances(std::span<const FStaticMeshInstance> Instances);
	void AccumulateInstancedDrawStatistics(const FInstancedDrawStatistics& Statistics);
	const FInstancedDrawStatistics& GetInstancedDrawStatistics() const { return InstancedDrawStatistics; }

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
	FBufferHandle StaticMeshInstanceBuffer;
	uint32 StaticMeshInstanceBufferCapacity = 0;
	FInstancedDrawStatistics InstancedDrawStatistics;

	TMap<FAssetId, std::unique_ptr<FMaterialResource>, FAssetIdHash> MaterialResources;
	TMap<FAssetId, std::unique_ptr<FStaticMeshResource>, FAssetIdHash> StaticMeshResources;
	TMap<FAssetId, std::unique_ptr<FTextureResource>, FAssetIdHash> TextureResources;

	FCommandListHandle CommandList;
};
