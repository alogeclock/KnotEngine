#include "Render/Resource/ShaderRegistry.h"

#include "Core/Assert.h"
#include "Core/IO/Paths.h"
#include "Render/RHI/RenderDevice.h"

#include <filesystem>
#include <fstream>
#include <limits>

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
}

FShaderHandle FShaderRegistry::GetOrCreate(const FShaderKey& Key)
{
	checkf(!Key.SourcePath.empty() && !Key.EntryPoint.empty(), "Shader Registry Key가 비어 있다.");
	for (const FEntry& Entry : Entries) // 교체해야 할 셰이더 수가 늘 경우 해시 맵으로 변경한다.
	{
		if (Entry.Key == Key)
		{
			return Entry.Handle;
		}
	}

	const TArray<uint8> ShaderSource = LoadSource(Key.SourcePath);
	FShaderHandle Handle = RenderDevice.CreateShader({ ShaderSource, Key.SourcePath, Key.EntryPoint, Key.Stage });
	panicf(Handle.IsValid(), "Shader Registry가 유효한 Shader Handle을 생성하지 못했다. Path={}, EntryPoint={}",
		Key.SourcePath, Key.EntryPoint);
	Entries.push_back({ Key, Handle });
	return Handle;
}

// Content 논리 경로의 Shader 소스 파일을 GPU Shader 생성에 사용할 바이트로 읽는다.
TArray<uint8> FShaderRegistry::LoadSource(const FString& SourcePath)
{
	const std::filesystem::path FilePath = FPaths::ResolveContentPath(SourcePath);
	std::ifstream Stream(FilePath, std::ios::binary | std::ios::ate);
	panicf(Stream, "Shader 소스 파일을 찾지 못했다. Path={}", SourcePath);

	const std::streamoff FileSize = Stream.tellg();
	panicf(FileSize > 0 && static_cast<uint64>(FileSize) <= (std::numeric_limits<SIZE_T>::max)(), "Shader 소스 파일 크기가 유효하지 않다. Path={}", SourcePath);

	TArray<uint8> Bytes(static_cast<SIZE_T>(FileSize));
	Stream.seekg(0, std::ios::beg);
	panicf(Stream.read(reinterpret_cast<char*>(Bytes.data()), FileSize), "Shader 소스 파일을 읽지 못했다. Path={}", SourcePath);
	return Bytes;
}

void FShaderRegistry::Release()
{
	for (FEntry& Entry : Entries)
	{
		RenderDevice.DestroyShader(Entry.Handle);
	}
	Entries.clear();
}
