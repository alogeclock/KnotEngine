#pragma once

#include "EngineAPI.h"

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

// Shader Compiler가 생성한 API Bytecode와 동일 Bytecode에서 추출한 Reflection이다.
struct ENGINE_API FCompiledShaderData
{
	TArray<uint8> Bytecode;
	FShaderReflection Reflection;
};

// Renderer 모듈의 플랫폼 Shader Compiler를 Engine의 Shader Registry에 연결하는 DLL 경계다.
class ENGINE_API IShaderCompiler
{
public:
	IShaderCompiler() = default;
	virtual ~IShaderCompiler() = default;

	IShaderCompiler(const IShaderCompiler&) = delete;
	IShaderCompiler& operator=(const IShaderCompiler&) = delete;
	IShaderCompiler(IShaderCompiler&&) = delete;
	IShaderCompiler& operator=(IShaderCompiler&&) = delete;

	virtual FCompiledShaderData Compile(const FShaderKey& Key, std::span<const uint8> Source) = 0;
};
