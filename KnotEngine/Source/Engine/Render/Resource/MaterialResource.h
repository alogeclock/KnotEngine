#pragma once

#include "EngineAPI.h"

#include "Asset/Material/MaterialParameters.h"
#include "Render/Shader/ShaderTypes.h"

class FMaterial;
class FTextureResource;
class URenderer;
class FShaderRegistry;
struct FMaterialResourceCommand;

// MaterialConstants를 Shader Stage의 b Register에 바인딩하는 위치다.
struct FMaterialConstantBufferBinding
{
	EShaderStage Stage = EShaderStage::Vertex;
	uint32 Slot = 0;
};

// Material Texture와 Sampler의 t/s Register 위치다.
struct FMaterialTextureBinding
{
	FName Name;
	EShaderStage Stage = EShaderStage::Pixel;
	uint32 TextureSlot = 0;
	uint32 SamplerSlot = FSamplerHandle::InvalidIndex;
};

// Shader Reflection을 Material Resource 바인딩으로 변환하는 임시 데이터다.
struct FMaterialParameterLayout
{
	TArray<FMaterialParameterDesc> Parameters;
	TArray<FMaterialConstantBufferBinding> ConstantBuffers;
	TArray<FMaterialTextureBinding> Textures;
	uint32 ConstantBufferSize = 0;
};

// Material Asset에서 생성한 Pipeline, 상수 및 Texture/Sampler 바인딩을 Renderer의 Resource Cache가 소유한다.
class ENGINE_API FMaterialResource final
{
public:
	struct FTextureBinding
	{
		EShaderStage Stage = EShaderStage::Pixel;
		uint32 TextureSlot = 0;
		uint32 SamplerSlot = FSamplerHandle::InvalidIndex;
		const FTextureResource* TextureResource = nullptr;
		FSamplerHandle Sampler;
	};

	bool Initialize(URenderer& Renderer, const FMaterialResourceCommand& Command, uint32 InSortId);
	void Release();

	FPipelineStateHandle GetPipelineState() const { return PipelineState; }
	const TArray<uint8>& GetConstants() const { return Constants; }
	const TArray<FMaterialConstantBufferBinding>& GetConstantBuffers() const { return ConstantBuffers; }
	const TArray<FTextureBinding>& GetTextures() const { return Textures; }

	uint32 GetSortId() const { return SortId; }
	uint64 GetSourceRevision() const { return SourceRevision; }
	bool IsValid() const { return PipelineState.IsValid() && SourceRevision != 0; }

private:
	static void BuildParameterLayout(FMaterialParameterLayout& Layout, FShaderRegistry& ShaderRegistry, const FMaterial& Material);
	static void AppendShaderReflection(FMaterialParameterLayout& Layout, const FShaderReflection& Reflection);

	uint32 SortId = 0;
	uint64 SourceRevision = 0;
	FPipelineStateHandle PipelineState;
	TArray<uint8> Constants;
	TArray<FMaterialConstantBufferBinding> ConstantBuffers;
	TArray<FTextureBinding> Textures;
};
