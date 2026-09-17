#pragma once

#include "EngineAPI.h"

#include "Render/RHI/RenderTypes.h"

class IRenderDevice;

// 동일한 Sampler Description에 대응하는 GPU Sampler 생성을 중복하지 않는다.
class ENGINE_API FSamplerStateCache
{
public:
	explicit FSamplerStateCache(IRenderDevice& InRenderDevice);
	~FSamplerStateCache();

	FSamplerStateCache(const FSamplerStateCache&) = delete;
	FSamplerStateCache& operator=(const FSamplerStateCache&) = delete;

	void Create();
	FSamplerHandle GetOrCreate(const FSamplerDesc& Desc);
	void Release();

private:
	struct FEntry
	{
		FSamplerDesc Desc;
		FSamplerHandle Handle;
	};

	IRenderDevice& RenderDevice;
	TArray<FEntry> Entries;
};
