#pragma once

#include "EngineAPI.h"

#include "Core/Archive/Archive.h"
#include "Render/RHI/RenderTypes.h"

#include <span>

// Content Shader 소스와 Entry Point, Stage, Permutation 조합으로 Shader Variant를 식별한다.
struct ENGINE_API FShaderKey
{
	FString SourcePath;
	FString EntryPoint;
	EShaderStage Stage = EShaderStage::Vertex;
	uint32 PermutationId = 0; // TODO: Permutation별 define을 Shader 컴파일에 적용한다.

	bool operator==(const FShaderKey&) const = default;
};

inline FArchive& operator<<(FArchive& Ar, FShaderKey& Key)
{
	Ar << Key.SourcePath;
	Ar << Key.EntryPoint;
	Ar << Key.Stage;
	Ar << Key.PermutationId;
	return Ar;
}

// 플랫폼 Shader Format에 전달할 소스와 Shader Variant 정보다. Source는 동기 Compile 호출 동안만 유효하다.
struct ENGINE_API FShaderCompilerInput
{
	FShaderKey Key;
	std::span<const uint8> Source;
};

// 플랫폼 Shader Format이 생성한 API Bytecode와 동일 Bytecode에서 추출한 Reflection이다.
struct ENGINE_API FShaderCompilerOutput
{
	TArray<uint8> Bytecode;
	FShaderReflection Reflection;
};

// 핫 리로드가 한 번에 교체할 Key와 컴파일 결과다.
struct ENGINE_API FShaderReloadEntry
{
	FShaderKey Key;
	FShaderCompilerOutput Output;
};
