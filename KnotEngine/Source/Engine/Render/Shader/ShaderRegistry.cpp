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

FShaderRegistry::FEntry& FShaderRegistry::GetOrCreateEntry(const FShaderKey& Key)
{
	checkf(!Key.SourcePath.empty() && !Key.EntryPoint.empty(), "Shader Registry Key가 비어 있다.");
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

void FShaderRegistry::Release()
{
	for (FEntry& Entry : Entries)
	{
		RenderDevice.DestroyShader(Entry.Handle);
	}
	Entries.clear();
}
