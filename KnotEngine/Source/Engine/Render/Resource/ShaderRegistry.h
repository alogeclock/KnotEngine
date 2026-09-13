#pragma once

#include "EngineAPI.h"

#include "Render/RHI/RenderTypes.h"

class IRenderDevice;

struct ENGINE_API FShaderKey
{
	uint32 ResourceId = 0; // Win32 리소스 시스템 기반 숫자 아이디
	FString SourceName;
	FString EntryPoint;
	EShaderStage Stage = EShaderStage::Vertex;

	bool operator==(const FShaderKey&) const = default;
};

// 내장 Shader를 Key별로 한 번 생성하고 Render Device 수명 동안 Handle을 소유한다.
class ENGINE_API FShaderRegistry
{
public:
	explicit FShaderRegistry(IRenderDevice& InRenderDevice);
	~FShaderRegistry();

	FShaderRegistry(const FShaderRegistry&) = delete;
	FShaderRegistry& operator=(const FShaderRegistry&) = delete;
	FShaderRegistry(FShaderRegistry&&) = delete;
	FShaderRegistry& operator=(FShaderRegistry&&) = delete;

	void Create();
	FShaderHandle GetOrCreate(const FShaderKey& Key);
	void Release();

private:
	struct FEntry
	{
		FShaderKey Key;
		FShaderHandle Handle;
	};

	IRenderDevice& RenderDevice;
	TArray<FEntry> Entries;
};

extern ENGINE_API FShaderRegistry* GShaderRegistry;
