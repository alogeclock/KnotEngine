#include "Render/Shader/ShaderRegistry.h"

#include "Core/Assert.h"
#include "Render/RHI/RenderDevice.h"
#include "Render/Shader/ShaderCompiler.h"

#include <algorithm>

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

// 교체 예정 Shader의 MaterialConstants와 Texture/Sampler 배치가 기존 Shader와 다른지 확인한다.
bool FShaderRegistry::HasStagedMaterialLayoutChange(FShaderHandle ExistingHandle) const
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
				return HasMaterialLayoutChange(Existing.Reflection, Staged.Reflection);
			}
		}
		return false;
	}
	return false;
}

// Material Resource 재패킹에 관여하는 Reflection만 순서와 무관하게 구조적으로 비교한다.
bool FShaderRegistry::HasMaterialLayoutChange(const FShaderReflection& Existing, const FShaderReflection& Staged)
{
	static const FName MaterialConstantsName("MaterialConstants");

	const auto IsMaterialConstants = [](const FShaderConstantBufferDesc& Buffer)
	{
		return Buffer.Name == MaterialConstantsName;
	};

	const auto HasSameParameter = [](const FShaderParameterDesc& Left, const FShaderParameterDesc& Right)
	{
		return Left.Name == Right.Name && Left.BaseType == Right.BaseType && Left.Class == Right.Class && Left.Offset == Right.Offset &&
			Left.Size == Right.Size && Left.Columns == Right.Columns && Left.Elements == Right.Elements;
	};

	const auto HasSameParameters = [&HasSameParameter](const FShaderConstantBufferDesc& Left, const FShaderConstantBufferDesc& Right)
	{
		if (Left.Parameters.size() != Right.Parameters.size())
		{
			return false;
		}
		return std::all_of(Left.Parameters.begin(), Left.Parameters.end(), [&Right, &HasSameParameter](const FShaderParameterDesc& Parameter)
		{
			return std::any_of(Right.Parameters.begin(), Right.Parameters.end(), [&Parameter, &HasSameParameter](const FShaderParameterDesc& Candidate)
			{
				return HasSameParameter(Parameter, Candidate);
			});
		});
	};

	const auto HasSameConstantBuffer = [&HasSameParameters](const FShaderConstantBufferDesc& Left, const FShaderConstantBufferDesc& Right)
	{
		return Left.Name == Right.Name && Left.Stage == Right.Stage && Left.Slot == Right.Slot && Left.Size == Right.Size &&
			HasSameParameters(Left, Right);
	};

	const SIZE_T ExistingBufferCount = static_cast<SIZE_T>(
		std::count_if(Existing.ConstantBuffers.begin(), Existing.ConstantBuffers.end(), IsMaterialConstants));
	const SIZE_T StagedBufferCount = static_cast<SIZE_T>(
		std::count_if(Staged.ConstantBuffers.begin(), Staged.ConstantBuffers.end(), IsMaterialConstants));
	if (ExistingBufferCount != StagedBufferCount)
	{
		return true;
	}
	for (const FShaderConstantBufferDesc& ExistingBuffer : Existing.ConstantBuffers)
	{
		if (!IsMaterialConstants(ExistingBuffer))
		{
			continue;
		}
		const bool bFound = std::any_of(
			Staged.ConstantBuffers.begin(),
			Staged.ConstantBuffers.end(),
			[&ExistingBuffer, &HasSameConstantBuffer](const FShaderConstantBufferDesc& StagedBuffer)
			{
				return HasSameConstantBuffer(ExistingBuffer, StagedBuffer);
			});
		if (!bFound)
		{
			return true;
		}
	}

	const auto IsTexture = [](const FShaderResourceBindingDesc& Resource)
	{
		return Resource.Type == EShaderResourceType::Texture2D || Resource.Type == EShaderResourceType::TextureCube;
	};

	const auto HasSampler = [](const FShaderReflection& Reflection, const FShaderResourceBindingDesc& Texture)
	{
		return std::any_of(Reflection.Resources.begin(), Reflection.Resources.end(), [&Texture](const FShaderResourceBindingDesc& Resource)
		{
			return Resource.Type == EShaderResourceType::Sampler && Resource.Stage == Texture.Stage && Resource.Slot == Texture.Slot;
		});
	};

	const SIZE_T ExistingTextureCount = static_cast<SIZE_T>(std::count_if(Existing.Resources.begin(), Existing.Resources.end(), IsTexture));
	const SIZE_T StagedTextureCount = static_cast<SIZE_T>(std::count_if(Staged.Resources.begin(), Staged.Resources.end(), IsTexture));
	if (ExistingTextureCount != StagedTextureCount)
	{
		return true;
	}

	for (const FShaderResourceBindingDesc& ExistingTexture : Existing.Resources)
	{
		if (!IsTexture(ExistingTexture))
		{
			continue;
		}
		const auto StagedTexture = std::find(Staged.Resources.begin(), Staged.Resources.end(), ExistingTexture);
		if (StagedTexture == Staged.Resources.end() || HasSampler(Existing, ExistingTexture) != HasSampler(Staged, *StagedTexture))
		{
			return true;
		}
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
