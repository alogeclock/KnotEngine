#pragma once

#include "EngineAPI.h"

#include "Render/Resource/Material/Material.h"

class UMaterialInterface;
class URenderer;

// Material UObject에서 한 번 생성한 Pipeline과 Shader Parameter 바인딩을 렌더 경로에서 재사용한다.
class ENGINE_API FMaterialRenderProxy final
{
public:
	struct FTextureBinding
	{
		EShaderStage Stage = EShaderStage::Pixel;
		uint32 TextureSlot = 0;
		uint32 SamplerSlot = FSamplerHandle::InvalidIndex;
		FTextureHandle Texture;
		FSamplerHandle Sampler;
	};

	explicit FMaterialRenderProxy(const UMaterialInterface* InMaterialInterface);

	void Register(URenderer& Renderer);

	FPipelineStateHandle GetPipelineState() const { return PipelineState; }
	const TArray<uint8>& GetConstants() const { return Constants; }
	const TArray<FMaterialConstantBufferBinding>& GetConstantBuffers() const { return ConstantBuffers; }
	const TArray<FTextureBinding>& GetTextures() const { return Textures; }
	const UMaterialInterface* GetMaterialInterface() const { return MaterialInterface; }
	bool IsRegistered() const { return PipelineState.IsValid(); }

private:
	const UMaterialInterface* MaterialInterface = nullptr;
	FPipelineStateHandle PipelineState;
	TArray<uint8> Constants;
	TArray<FMaterialConstantBufferBinding> ConstantBuffers;
	TArray<FTextureBinding> Textures;
};
