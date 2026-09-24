#include "Render/Shader/ShaderRegistry.h"

#include "Core/Assert.h"
#include "Render/RHI/RenderDevice.h"
#include "Render/Shader/ShaderCompiler.h"

FShaderRegistry::FShaderRegistry(IRenderDevice& InRenderDevice, FShaderCompiler& InShaderCompiler)
	: RenderDevice(InRenderDevice), ShaderCompiler(InShaderCompiler)
{
}

FShaderRegistry::~FShaderRegistry()
{
	Release();
}

void FShaderRegistry::Create()
{
	Release();
}

FShaderHandle FShaderRegistry::GetOrCreate(const FShaderKey& Key)
{
	return GetOrCreateEntry(Key).Handle;
}

const FShaderReflection& FShaderRegistry::GetReflection(const FShaderKey& Key)
{
	return GetOrCreateEntry(Key).Reflection;
}

// 등록된 Shader 중 지정한 소스 경로를 사용하는 Key를 모은다.
TArray<FShaderKey> FShaderRegistry::GetKeysForSource(const FString& SourcePath) const
{
	TArray<FShaderKey> Keys;
	for (const FEntry& Entry : Entries)
	{
		if (SourcePath.empty() || Entry.Key.SourcePath == SourcePath)
		{
			Keys.push_back(Entry.Key);
		}
	}
	return Keys;
}

// 컴파일 결과로 새 GPU Shader를 생성해 기존 Entry와 분리해 보관한다.
bool FShaderRegistry::StageReload(const TArray<FShaderReloadEntry>& Compiled, FString& Diagnostics)
{
	CancelReload();
	for (const FShaderReloadEntry& Entry : Compiled)
	{
		FShaderHandle Handle;
		if (!RenderDevice.TryCreateShader({ Entry.Output.Bytecode, Entry.Key.SourcePath + ":" + Entry.Key.EntryPoint, Entry.Key.Stage }, Handle, Diagnostics))
		{
			CancelReload();
			return false;
		}
		StagedEntries.push_back({ Entry.Key, Handle, Entry.Output.Reflection });
	}
	if (StagedEntries.empty())
	{
		Diagnostics = "다시 컴파일할 Shader Key가 없다.";
		return false;
	}
	return true;
}

// 준비된 Shader Handle과 Reflection을 등록 Entry에 반영하고 옛 Handle을 보관한다.
void FShaderRegistry::CommitReload()
{
	for (FEntry& Staged : StagedEntries)
	{
		bool bFound = false;
		for (FEntry& Entry : Entries)
		{
			if (Entry.Key == Staged.Key)
			{
				bFound = true;
				ReplacedHandles.push_back(Entry.Handle);
				Entry.Handle = Staged.Handle;
				Entry.Reflection = std::move(Staged.Reflection);
				Staged.Handle.Reset();
				break;
			}
		}
		checkf(bFound, "등록되지 않은 Shader Key를 교체할 수 없다. Path={}, EntryPoint={}", Staged.Key.SourcePath, Staged.Key.EntryPoint);
	}
	StagedEntries.clear();
}

// Shader Reload에 실패하면 준비된 GPU Shader만 해제한다.
void FShaderRegistry::CancelReload()
{
	for (FEntry& Entry : StagedEntries)
	{
		RenderDevice.DestroyShader(Entry.Handle);
	}
	StagedEntries.clear();
}

// 교체 후 폐기할 이전 Shader Handle을 반환한다.
TArray<FShaderHandle> FShaderRegistry::GetReplacedHandles() const
{
	return ReplacedHandles;
}

// 옛 Shader와 준비된 새 Shader의 Handle 대응 관계를 반환한다.
TArray<std::pair<FShaderHandle, FShaderHandle>> FShaderRegistry::GetStagedReplacements() const
{
	TArray<std::pair<FShaderHandle, FShaderHandle>> Replacements;
	for (const FEntry& Staged : StagedEntries)
	{
		for (const FEntry& Entry : Entries)
		{
			if (Entry.Key == Staged.Key)
			{
				Replacements.emplace_back(Entry.Handle, Staged.Handle);
				break;
			}
		}
	}
	return Replacements;
}

// 교체 예정 Shader의 Reflection이 기존 Shader와 다른지 확인한다.
bool FShaderRegistry::HasStagedReflectionChange(FShaderHandle ExistingHandle) const
{
	for (const FEntry& Existing : Entries)
	{
		if (Existing.Handle != ExistingHandle)
		{
			continue;
		}
		for (const FEntry& Staged : StagedEntries)
		{
			if (Staged.Key == Existing.Key)
			{
				return Staged.Reflection != Existing.Reflection;
			}
		}
		return false;
	}
	return false;
}

// GPU 사용 종료를 확인한 이전 Shader Handle을 해제한다.
void FShaderRegistry::ReleaseReplacedShaders()
{
	for (FShaderHandle& Handle : ReplacedHandles)
	{
		RenderDevice.DestroyShader(Handle);
	}
	ReplacedHandles.clear();
}

// Reload 중에는 준비된 Entry를 우선 조회해 새 Shader와 Reflection을 사용한다.
FShaderRegistry::FEntry& FShaderRegistry::GetOrCreateEntry(const FShaderKey& Key)
{
	checkf(!Key.SourcePath.empty() && !Key.EntryPoint.empty(), "Shader Registry Key가 비어 있다.");
	for (FEntry& Entry : StagedEntries)
	{
		if (Entry.Key == Key)
		{
			return Entry;
		}
	}
	for (FEntry& Entry : Entries) // 교체해야 할 셰이더 수가 늘 경우 해시 맵으로 변경한다.
	{
		if (Entry.Key == Key)
		{
			return Entry;
		}
	}

	FShaderCompilerOutput Output = ShaderCompiler.GetOrCompile(Key);
	FShaderHandle Handle = RenderDevice.CreateShader({ Output.Bytecode, Key.SourcePath + ":" + Key.EntryPoint, Key.Stage });
	panicf(Handle.IsValid(), "Shader Registry가 유효한 Shader Handle을 생성하지 못했다. Path={}, EntryPoint={}", Key.SourcePath, Key.EntryPoint);
	Entries.push_back({ Key, Handle, std::move(Output.Reflection) });
	return Entries.back();
}

// 준비·교체·등록 단계에 남아 있는 모든 Shader Handle을 해제한다.
void FShaderRegistry::Release()
{
	CancelReload();
	ReleaseReplacedShaders();
	for (FEntry& Entry : Entries)
	{
		RenderDevice.DestroyShader(Entry.Handle);
	}
	Entries.clear();
}
