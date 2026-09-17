#pragma once

#include "EngineAPI.h"

#include "Render/Shader/ShaderTypes.h"

// 플랫폼별 Shader 출력 형식과 Compiler 구현을 Engine의 Shader Compiler에 연결하는 DLL 경계다.
class ENGINE_API IShaderFormat
{
public:
	IShaderFormat() = default;
	virtual ~IShaderFormat() = default;

	IShaderFormat(const IShaderFormat&) = delete;
	IShaderFormat& operator=(const IShaderFormat&) = delete;
	IShaderFormat(IShaderFormat&&) = delete;
	IShaderFormat& operator=(IShaderFormat&&) = delete;

	virtual FName GetName() const = 0;
	virtual uint32 GetVersion() const = 0;
	virtual FShaderCompilerOutput Compile(const FShaderCompilerInput& Input) = 0;
};
