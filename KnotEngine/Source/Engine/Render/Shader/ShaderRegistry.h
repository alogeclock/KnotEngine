#pragma once

#include "EngineAPI.h"

#include "Render/Shader/ShaderCompiler.h"

#include <filesystem>
#include <span>

class IRenderDevice;

// Shader Key별 GPU Shader를 최초 요청 시 생성하고 Render Device 수명 동안 Handle을 소유한다.
// 필요 시 Global Shader Registry와 Material Shader Registry를 구분하도록 수정한다.
class ENGINE_API FShaderRegistry
{
public:
	FShaderRegistry(IRenderDevice& InRenderDevice, IShaderCompiler* InShaderCompiler);
	~FShaderRegistry();

	FShaderRegistry(const FShaderRegistry&) = delete;
	FShaderRegistry& operator=(const FShaderRegistry&) = delete;
	FShaderRegistry(FShaderRegistry&&) = delete;
	FShaderRegistry& operator=(FShaderRegistry&&) = delete;

	void Create();
	void Release();

	FShaderHandle GetOrCreate(const FShaderKey& Key);
	const FShaderReflection& GetReflection(const FShaderKey& Key);

private:
	struct FEntry
	{
		FShaderKey Key;
		FShaderHandle Handle;
		FShaderReflection Reflection;
	};

	static TArray<uint8> LoadFile(const std::filesystem::path& FilePath);
	static uint64 HashKey(const FShaderKey& Key);
	static uint64 HashSource(const FShaderKey& Key, std::span<const uint8> Source);

	static FString GetShaderAssetFileName(const FShaderKey& Key);
	static std::filesystem::path GetShaderCachePath(const FShaderKey& Key);
	static bool LoadCookedShader(const std::filesystem::path& FilePath, uint64 KeyHash, uint64 SourceHash, FCompiledShaderData& OutData);
	static void SaveCookedShader(const std::filesystem::path& FilePath, uint64 KeyHash, uint64 SourceHash, const FCompiledShaderData& Data);

	FCompiledShaderData LoadOrCompile(const FShaderKey& Key);
	FEntry& GetOrCreateEntry(const FShaderKey& Key);

	IRenderDevice& RenderDevice;
	IShaderCompiler* ShaderCompiler = nullptr;
	TArray<FEntry> Entries;
};
