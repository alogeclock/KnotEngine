#pragma once

#include "RendererAPI.h"

#include "Render/Shader/ShaderCompiler.h"

struct ID3D10Blob;

// HLSL을 D3D11 Shader Model 5 Bytecode로 컴파일하고 Reflection 메타데이터를 추출한다.
class RENDERER_API FD3D11ShaderCompiler final : public IShaderCompiler
{
public:
	FCompiledShaderData Compile(const FShaderKey& Key, std::span<const uint8> Source) override;

private:
	static FShaderReflection ReflectShader(ID3D10Blob& Bytecode, EShaderStage Stage);
};
