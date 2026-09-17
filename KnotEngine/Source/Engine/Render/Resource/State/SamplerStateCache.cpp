#include "Render/Resource/State/SamplerStateCache.h"

#include "Core/Assert.h"
#include "Render/RHI/RenderDevice.h"

FSamplerStateCache::FSamplerStateCache(IRenderDevice& InRenderDevice)
	: RenderDevice(InRenderDevice)
{
}

FSamplerStateCache::~FSamplerStateCache()
{
	Release();
}

void FSamplerStateCache::Create()
{
	Release();
}

FSamplerHandle FSamplerStateCache::GetOrCreate(const FSamplerDesc& Desc)
{
	for (const FEntry& Entry : Entries)
	{
		if (Entry.Desc == Desc)
		{
			return Entry.Handle;
		}
	}

	FSamplerHandle Handle = RenderDevice.CreateSampler(Desc);
	panicf(Handle.IsValid(), "Sampler State Cache가 유효한 Handle을 생성하지 못했다.");
	Entries.push_back({ Desc, Handle });
	return Handle;
}

void FSamplerStateCache::Release()
{
	for (FEntry& Entry : Entries)
	{
		RenderDevice.DestroySampler(Entry.Handle);
	}
	Entries.clear();
}
