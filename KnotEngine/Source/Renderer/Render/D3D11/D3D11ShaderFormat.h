#pragma once

#include "RendererAPI.h"

#include "Render/Shader/ShaderFormat.h"

struct ID3D10Blob;

// HLSL을 D3D11 Shader Model 5 DXBC로 컴파일하고 Reflection 메타데이터를 추출한다.
class RENDERER_API FD3D11ShaderFormat final : public IShaderFormat
{
public:
	FName GetName() const override { return FName("D3D11"); }
	uint32 GetVersion() const override { return 1; }
	FShaderCompilerOutput Compile(const FShaderCompilerInput& Input) override;

private:
	static FShaderReflection ReflectShader(ID3D10Blob& Bytecode, EShaderStage Stage);
};
