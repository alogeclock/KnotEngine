#include "Render/Resource/State/PipelineStateCache.h"

#include "Core/Assert.h"
#include "Render/RHI/RenderDevice.h"

#include <algorithm>
#include <iterator>

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

// 일반 생성 경로에서 Pipeline 생성 실패를 치명적 오류로 처리한다.
FPipelineStateHandle FPipelineStateCache::GetOrCreate(const FPipelineStateDesc& Desc)
{
	FPipelineStateHandle Handle;
	FString Diagnostics;
	panicf(TryGetOrCreate(Desc, Handle, Diagnostics), "Pipeline State 생성 실패: {}", Diagnostics);
	return Handle;
}

// 기존 또는 준비 중인 Pipeline을 재사용하고 새 자원 생성 실패를 호출자에게 돌려준다.
bool FPipelineStateCache::TryGetOrCreate(const FPipelineStateDesc& Desc, FPipelineStateHandle& Handle, FString& Diagnostics)
{
	for (const FEntry& Entry : StagedEntries)
	{
		if (Entry.Desc == Desc)
		{
			Handle = Entry.Handle;
			return true;
		}
	}
	for (const FEntry& Entry : Entries) // 교체해야 할 PSO 수가 늘 경우 해시 맵으로 변경
	{
		if (Entry.Desc == Desc)
		{
			Handle = Entry.Handle;
			return true;
		}
	}

	if (!RenderDevice.TryCreatePipelineState(Desc, Handle, Diagnostics))
	{
		return false;
	}
	(bReloading ? StagedEntries : Entries).push_back({ Desc, Handle });
	return true;
}

// Shader 교체에 사용할 Pipeline 후보를 별도 목록에 저장하기 시작한다.
void FPipelineStateCache::BeginReload()
{
	check(!bReloading && StagedEntries.empty());
	bReloading = true;
}

// 교체 대상 Shader를 참조하는 기존 Pipeline의 새 Handle을 미리 생성한다.
bool FPipelineStateCache::StageAffectedPipelines(std::span<const std::pair<FShaderHandle, FShaderHandle>> Replacements, FString& Diagnostics)
{
	check(bReloading);
	for (const FEntry& Entry : Entries)
	{
		FPipelineStateDesc Desc = Entry.Desc;
		bool bAffected = false;
		for (const auto& [OldShader, NewShader] : Replacements)
		{
			if (Desc.VertexShader == OldShader)
			{
				Desc.VertexShader = NewShader;
				bAffected = true;
			}
			if (Desc.PixelShader == OldShader)
			{
				Desc.PixelShader = NewShader;
				bAffected = true;
			}
		}
		if (bAffected)
		{
			FPipelineStateHandle Handle;
			if (!TryGetOrCreate(Desc, Handle, Diagnostics))
			{
				return false;
			}
		}
	}
	return true;
}

// Shader Reload가 실패하면 준비 중인 Pipeline만 폐기한다.
void FPipelineStateCache::CancelReload()
{
	bReloading = false;
	for (FEntry& Entry : StagedEntries)
	{
		RenderDevice.DestroyPipelineState(Entry.Handle);
	}
	StagedEntries.clear();
}

// 교체된 Shader를 참조하는 옛 Pipeline을 제거하고 준비된 Pipeline을 등록한다.
void FPipelineStateCache::CommitReload(std::span<const FShaderHandle> ReplacedShaders)
{
	check(bReloading);
	bReloading = false;
	for (FEntry& Entry : Entries)
	{
		const bool bReplaced = std::find(ReplacedShaders.begin(), ReplacedShaders.end(), Entry.Desc.VertexShader) != ReplacedShaders.end() ||
			std::find(ReplacedShaders.begin(), ReplacedShaders.end(), Entry.Desc.PixelShader) != ReplacedShaders.end();
		if (bReplaced)
		{
			RenderDevice.DestroyPipelineState(Entry.Handle);
		}
	}
	std::erase_if(Entries, [](const FEntry& Entry) { return !Entry.Handle.IsValid(); });
	Entries.insert(Entries.end(), std::make_move_iterator(StagedEntries.begin()), std::make_move_iterator(StagedEntries.end()));
	StagedEntries.clear();
	// 변형 키는 기존 기본 PSO Handle에 묶인다. 다음 Draw에서 필요한 변형을 다시 생성한다.
	WireframeVariants.clear();
}

// 준비 중인 후보와 등록된 Pipeline을 모두 해제한다.
void FPipelineStateCache::Release()
{
	CancelReload();
	WireframeVariants.clear();
	for (FEntry& Entry : Entries)
	{
		RenderDevice.DestroyPipelineState(Entry.Handle);
	}
	Entries.clear();
}
