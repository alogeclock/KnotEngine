#pragma once

#include "EngineAPI.h"

#include "Render/Shader/ShaderTypes.h"

class FShaderCompiler;
class IRenderDevice;

// Shader Key별 GPU Shader를 최초 요청 시 생성하고 Render Device 수명 동안 Handle을 소유한다.
// 필요 시 Global Shader Registry와 Material Shader Registry를 구분하도록 수정한다.
class ENGINE_API FShaderRegistry
{
public:
	FShaderRegistry(IRenderDevice& InRenderDevice, FShaderCompiler& InShaderCompiler);
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

	FEntry& GetOrCreateEntry(const FShaderKey& Key);

	IRenderDevice& RenderDevice;
	FShaderCompiler& ShaderCompiler;
	TArray<FEntry> Entries;
};
