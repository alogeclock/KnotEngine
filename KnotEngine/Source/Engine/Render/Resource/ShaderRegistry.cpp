#include "Render/Resource/ShaderRegistry.h"

#include "Core/Assert.h"
#include "Render/RHI/RenderDevice.h"

#include <Windows.h>

FShaderRegistry* GShaderRegistry = nullptr;

FShaderRegistry::FShaderRegistry(IRenderDevice& InRenderDevice)
    : RenderDevice(InRenderDevice)
{
}

FShaderRegistry::~FShaderRegistry()
{
	Release();
}

void FShaderRegistry::Create()
{
	Release();
	checkf(!GShaderRegistry, "Shader Registry가 이미 생성되어 있다.");
	GShaderRegistry = this;
}

FShaderHandle FShaderRegistry::GetOrCreate(const FShaderKey& Key)
{
	checkf(GShaderRegistry == this && Key.ResourceId != 0 && !Key.EntryPoint.empty(), "Registry가 생성되지 않았고, Shader Registry Key가 비어 있다.");
	for (const FEntry& Entry : Entries) // 교체해야 할 셰이더 수가 늘 경우 해시 맵으로 변경
	{
		if (Entry.Key == Key)
		{
			return Entry.Handle;
		}
	}

	HMODULE Module = GetModuleHandleW(nullptr);
	HRSRC ResourceInfo = FindResourceW(Module, MAKEINTRESOURCEW(Key.ResourceId), RT_RCDATA);
	panicf(ResourceInfo, "Shader 내장 리소스를 찾지 못했습니다. ResourceId={}", Key.ResourceId);
	HGLOBAL ResourceData = LoadResource(Module, ResourceInfo);
	panicf(ResourceData, "Shader 내장 리소스를 불러오지 못했습니다. ResourceId={}", Key.ResourceId);
	const DWORD ResourceSize = SizeofResource(Module, ResourceInfo);
	const auto* Bytes = static_cast<const uint8*>(LockResource(ResourceData));
	panicf(Bytes && ResourceSize > 0, "Shader 내장 리소스 데이터가 비어 있습니다. ResourceId={}", Key.ResourceId);

	const std::span<const uint8> ShaderSource(Bytes, ResourceSize);
	FShaderHandle Handle = RenderDevice.CreateShader({ ShaderSource, Key.SourceName, Key.EntryPoint, Key.Stage });
	panicf(Handle.IsValid(), "Shader Registry가 유효한 Shader Handle을 생성하지 못했다. ResourceId={}, EntryPoint={}", Key.ResourceId, Key.EntryPoint);
	Entries.push_back({ Key, Handle });
	return Handle;
}

void FShaderRegistry::Release()
{
	if (GShaderRegistry != this)
	{
		return;
	}
	for (FEntry& Entry : Entries)
	{
		RenderDevice.DestroyShader(Entry.Handle);
	}
	Entries.clear();
	GShaderRegistry = nullptr;
}
