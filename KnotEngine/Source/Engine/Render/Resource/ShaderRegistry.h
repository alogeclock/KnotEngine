#pragma once

#include "EngineAPI.h"

#include "Render/RHI/RenderTypes.h"

class IRenderDevice;

// Content Shader 소스와 Entry Point, Stage, Permutation 조합으로 Shader Variant를 식별한다.
struct ENGINE_API FShaderKey
{
	FString SourcePath;
	FString EntryPoint;
	EShaderStage Stage = EShaderStage::Vertex;
	uint32 PermutationId = 0; // TODO: Permutation별 define을 Shader 컴파일에 적용한다.

	bool operator==(const FShaderKey&) const = default;
};

// Shader Key별 GPU Shader를 최초 요청 시 생성하고 Render Device 수명 동안 Handle을 소유한다.
// 필요 시 Global Shader Registry와 Material Shader Registry를 구분하도록 수정한다.
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
	void Release();

	FShaderHandle GetOrCreate(const FShaderKey& Key);

private:
	struct FEntry
	{
		FShaderKey Key;
		FShaderHandle Handle;
	};

	static TArray<uint8> LoadSource(const FString& SourcePath);

	IRenderDevice& RenderDevice;
	TArray<FEntry> Entries;
};
