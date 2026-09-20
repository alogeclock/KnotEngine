#include "Render/Proxy/MaterialRenderProxy.h"

#include "Asset/Material/MaterialInterface.h"
#include "Asset/Texture/Texture.h"
#include "Core/Assert.h"
#include "Render/Renderer.h"
#include "Render/Resource/Mesh/Vertex.h"
#include "Render/Resource/Texture.h"

#include <cstring>

FMaterialRenderProxy::FMaterialRenderProxy(const UMaterialInterface* InMaterialInterface, uint32 InSortId)
	: MaterialInterface(InMaterialInterface), SortId(InSortId)
{
}

// Material의 고정 Pipeline, 상수 및 Texture/Sampler 바인딩을 Renderer 자원으로 등록한다.
void FMaterialRenderProxy::Register(URenderer& Renderer)
{
	check(!IsRegistered());

	static const FShaderKey DefaultVertexShader{ "/Engine/Shader/StaticMesh.hlsl", "MainVS", EShaderStage::Vertex };
	static const FShaderKey DefaultPixelShader{ "/Engine/Shader/StaticMesh.hlsl", "OpaquePS", EShaderStage::Pixel };
	FMaterial DefaultMaterial;
	verify(DefaultMaterial.Initialize(DefaultVertexShader, DefaultPixelShader));

	const FMaterial* AssetMaterial = MaterialInterface ? MaterialInterface->GetMaterial() : nullptr;
	const bool bHasMaterialAsset = AssetMaterial && AssetMaterial->IsValid();
	const FMaterial& Material = bHasMaterialAsset ? *AssetMaterial : DefaultMaterial;
	FShaderRegistry& ShaderRegistry = Renderer.GetShaderRegistry();

	FPipelineStateDesc PipelineStateDesc;
	PipelineStateDesc.VertexShader = ShaderRegistry.GetOrCreate(Material.GetVertexShader());
	PipelineStateDesc.PixelShader = ShaderRegistry.GetOrCreate(Material.GetPixelShader());
	PipelineStateDesc.VertexLayout = FStaticMeshVertex::GetVertexLayout();
	PipelineStateDesc.RasterizerState.CullMode = Material.GetCullMode();
	PipelineStateDesc.DepthMode = Material.GetDepthMode();
	if (Material.GetBlendMode() == EMaterialBlendMode::Translucent)
	{
		FRenderTargetBlendDesc& Blend = PipelineStateDesc.BlendState.RenderTarget;
		Blend.bBlendEnabled = true;
		Blend.SourceColorBlend = EBlendFactor::SourceAlpha;
		Blend.DestinationColorBlend = EBlendFactor::InverseSourceAlpha;
		Blend.SourceAlphaBlend = EBlendFactor::One;
		Blend.DestinationAlphaBlend = EBlendFactor::InverseSourceAlpha;
	}
	PipelineState = Renderer.GetPipelineStateCache().GetOrCreate(PipelineStateDesc);
	check(PipelineState.IsValid());

	const FMaterialParameterLayout& Layout = Material.GetOrCreateParameterLayout(ShaderRegistry);
	ConstantBuffers = Layout.ConstantBuffers;
	if (bHasMaterialAsset)
	{
		MaterialInterface->PackMaterialConstants(Layout, Constants);
	}
	else
	{
		static const FName BaseColorName("BaseColor");
		static const FVector4 DefaultBaseColor(1.0f, 0.0f, 1.0f, 1.0f);
		Constants.assign(Layout.ConstantBufferSize, 0);
		for (const FMaterialParameterDesc& Parameter : Layout.Parameters)
		{
			if (Parameter.Name == BaseColorName && Parameter.Type == EMaterialParameterType::Vector4)
			{
				std::memcpy(Constants.data() + Parameter.Offset, DefaultBaseColor.Data, sizeof(DefaultBaseColor));
			}
		}
	}

	Textures.reserve(Layout.Textures.size());
	for (const FMaterialTextureBinding& Binding : Layout.Textures)
	{
		FTextureBinding TextureBinding;
		TextureBinding.Stage = Binding.Stage;
		TextureBinding.TextureSlot = Binding.TextureSlot;
		TextureBinding.SamplerSlot = Binding.SamplerSlot;
		TextureBinding.Texture = Renderer.GetDefaultTexture();
		FSamplerDesc Sampler;
		if (bHasMaterialAsset)
		{
			if (const FTextureMaterialParameter* Parameter = MaterialInterface->FindTextureParameter(Binding.Name); Parameter && Parameter->Texture)
			{
				panicf(Parameter->Texture->InitResources(Renderer.GetRenderDevice()), "Material Texture의 GPU Resource 생성에 실패했다. Name={}", Binding.Name.ToString());
				check(Parameter->Texture->GetResource() && Parameter->Texture->GetResource()->IsValid());
				TextureBinding.Texture = Parameter->Texture->GetResource()->GetHandle();
				Sampler = Parameter->Sampler;
			}
		}
		if (Binding.SamplerSlot != FSamplerHandle::InvalidIndex)
		{
			TextureBinding.Sampler = Renderer.GetSamplerStateCache().GetOrCreate(Sampler);
		}
		Textures.push_back(TextureBinding);
	}
}
