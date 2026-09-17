#pragma once

#include "EngineAPI.h"

#include "Render/Shader/ShaderFormat.h"

#include <filesystem>
#include <span>

// Shader 소스 로딩, 캐시 검증과 저장을 담당하고 cache miss일 때 플랫폼 Shader Format을 호출한다.
class ENGINE_API FShaderCompiler
{
public:
	explicit FShaderCompiler(IShaderFormat& InShaderFormat);

	FShaderCompilerOutput GetOrCompile(const FShaderKey& Key);

private:
	static TArray<uint8> LoadFile(const std::filesystem::path& FilePath);
	uint64 HashKey(const FShaderKey& Key) const;
	uint64 HashSource(const FShaderKey& Key, std::span<const uint8> Source) const;

	FString GetShaderAssetFileName(const FShaderKey& Key) const;
	std::filesystem::path GetShaderCachePath(const FShaderKey& Key) const;
	static bool LoadCookedShader(const std::filesystem::path& FilePath, uint64 KeyHash, uint64 SourceHash, FShaderCompilerOutput& OutOutput);
	static void SaveCookedShader(const std::filesystem::path& FilePath, uint64 KeyHash, uint64 SourceHash, const FShaderCompilerOutput& Output);

	IShaderFormat& ShaderFormat;
};
