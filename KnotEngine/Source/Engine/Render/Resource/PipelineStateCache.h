#pragma once

#include "EngineAPI.h"

#include "Render/RHI/RenderTypes.h"

class IRenderDevice;

// 완전한 Pipeline State Description을 비교해 동일한 PSO 생성을 중복하지 않는다.
class ENGINE_API FPipelineStateCache
{
public:
	explicit FPipelineStateCache(IRenderDevice& InRenderDevice);
	~FPipelineStateCache();

	FPipelineStateCache(const FPipelineStateCache&) = delete;
	FPipelineStateCache& operator=(const FPipelineStateCache&) = delete;
	FPipelineStateCache(FPipelineStateCache&&) = delete;
	FPipelineStateCache& operator=(FPipelineStateCache&&) = delete;

	void Create();
	FPipelineStateHandle GetOrCreate(const FPipelineStateDesc& Desc);
	void Release();

private:
	struct FEntry
	{
		FPipelineStateDesc Desc;
		FPipelineStateHandle Handle;
	};

	IRenderDevice& RenderDevice;
	TArray<FEntry> Entries;
};

extern ENGINE_API FPipelineStateCache* GPipelineStateCache;
