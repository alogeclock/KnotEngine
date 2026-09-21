#pragma once

#include "EngineAPI.h"
#include "Core/Assert.h"

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
	// 콜백은 저장하지 않고 캐시 미스일 때만 즉시 실행한다.
	template <typename FCreateDesc>
	FPipelineStateHandle GetOrCreateWireframe(FPipelineStateHandle BasePipeline, bool bOverlay, FCreateDesc&& CreateDesc)
	{
		check(BasePipeline.IsValid());
		const uint64 Key = (static_cast<uint64>(BasePipeline.Generation) << 32) | BasePipeline.Index;
		FPipelineStateHandle& Variant = WireframeVariants[Key][bOverlay ? 1 : 0];
		if (!Variant.IsValid())
		{
			Variant = GetOrCreate(CreateDesc());
		}
		return Variant;
	}
	void Release();

private:
	struct FEntry
	{
		FPipelineStateDesc Desc;
		FPipelineStateHandle Handle;
	};

	IRenderDevice& RenderDevice;
	TArray<FEntry> Entries;
	// 기본 PSO의 Generation과 Index를 키로 사용한다. 변형은 Entries가 소유한다.
	TMap<uint64, TStaticArray<FPipelineStateHandle, 2>> WireframeVariants;
};
