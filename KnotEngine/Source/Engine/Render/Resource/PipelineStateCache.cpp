#include "Render/Resource/PipelineStateCache.h"

#include "Core/Assert.h"
#include "Render/RHI/RenderDevice.h"

FPipelineStateCache* GPipelineStateCache = nullptr;

FPipelineStateCache::FPipelineStateCache(IRenderDevice& InRenderDevice)
	: RenderDevice(InRenderDevice)
{
}

FPipelineStateCache::~FPipelineStateCache()
{
	Release();
}

void FPipelineStateCache::Create()
{
	Release();
	checkf(!GPipelineStateCache, "Pipeline State Cache가 이미 생성되어 있다.");
	GPipelineStateCache = this;
}

FPipelineStateHandle FPipelineStateCache::GetOrCreate(const FPipelineStateDesc& Desc)
{
	check(GPipelineStateCache == this);
	for (const FEntry& Entry : Entries) // 교체해야 할 PSO 수가 늘 경우 해시 맵으로 변경
	{
		if (Entry.Desc == Desc)
		{
			return Entry.Handle;
		}
	}

	FPipelineStateHandle Handle = RenderDevice.CreatePipelineState(Desc);
	panicf(Handle.IsValid(), "Pipeline State Cache가 유효한 Handle을 생성하지 못했다.");
	Entries.push_back({ Desc, Handle });
	return Handle;
}

void FPipelineStateCache::Release()
{
	if (GPipelineStateCache != this)
	{
		return;
	}
	for (FEntry& Entry : Entries)
	{
		RenderDevice.DestroyPipelineState(Entry.Handle);
	}
	Entries.clear();
	GPipelineStateCache = nullptr;
}
