#include "Render/Resource/State/PipelineStateCache.h"

#include "Core/Assert.h"
#include "Render/RHI/RenderDevice.h"

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
}

FPipelineStateHandle FPipelineStateCache::GetOrCreate(const FPipelineStateDesc& Desc)
{
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
	for (FEntry& Entry : Entries)
	{
		RenderDevice.DestroyPipelineState(Entry.Handle);
	}
	Entries.clear();
}
