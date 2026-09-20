#include "Render/Resource/MaterialResource.h"

#include "Asset/Material/Material.h"
#include "Core/Assert.h"
#include "Render/Renderer.h"
#include "Render/Resource/ResourceCommand.h"
#include "Render/Resource/Mesh/Vertex.h"
#include "Render/Resource/TextureResource.h"

#include <algorithm>
#include <cstring>

// Material의 고정 Pipeline, 상수 및 Texture/Sampler 바인딩을 Renderer Resource로 생성한다.
bool FMaterialResource::Initialize(
	URenderer& Renderer,
	const FMaterialResourceCommand& Command,
	uint32 InSortId)
{
	Release();
	if (Command.Revision == 0)
	{
		return false;
	}

	static const FShaderKey DefaultVertexShader{ "/Engine/Shader/StaticMesh.hlsl", "MainVS", EShaderStage::Vertex };
	static const FShaderKey DefaultPixelShader{ "/Engine/Shader/StaticMesh.hlsl", "OpaquePS", EShaderStage::Pixel };
	FMaterial DefaultMaterial;
	verify(DefaultMaterial.Initialize(DefaultVertexShader, DefaultPixelShader));

	const bool bHasMaterialAsset = Command.Material.IsValid();
	const FMaterial& Material = bHasMaterialAsset ? Command.Material : DefaultMaterial;
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
	if (!PipelineState.IsValid())
	{
		return false;
	}

	FMaterialParameterLayout Layout;
	BuildParameterLayout(Layout, ShaderRegistry, Material);
	ConstantBuffers = Layout.ConstantBuffers;
	if (bHasMaterialAsset)
	{
		Constants.assign(Layout.ConstantBufferSize, 0);
		for (const FMaterialParameterDesc& Parameter : Layout.Parameters)
		{
			check(Parameter.Offset + Parameter.Size <= Constants.size());
			uint8* Destination = Constants.data() + Parameter.Offset;
			if (Parameter.Type == EMaterialParameterType::Scalar)
			{
				for (const FScalarMaterialParameter& Value : Command.ScalarParameters)
				{
					if (Value.Name != Parameter.Name)
					{
						continue;
					}
					check(Parameter.Size == sizeof(float));
					std::memcpy(Destination, &Value.Value, sizeof(float));
					break;
				}
				continue;
			}

			for (const FVectorMaterialParameter& Value : Command.VectorParameters)
			{
				if (Value.Name != Parameter.Name)
				{
					continue;
				}
				check(Parameter.Size <= sizeof(FVector4));
				std::memcpy(Destination, Value.Value.Data, Parameter.Size);
				break;
			}
		}
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
		TextureBinding.TextureResource = &Renderer.GetDefaultTextureResource();
		FSamplerDesc Sampler;
		if (bHasMaterialAsset)
		{
			for (const FMaterialTextureData& Texture : Command.Textures)
			{
				if (Texture.Name != Binding.Name)
				{
					continue;
				}
				FTextureResource* TextureResource = Renderer.FindTextureResource(Texture.AssetId);
				check(TextureResource && TextureResource->IsValid());
				TextureBinding.TextureResource = TextureResource;
				Sampler = Texture.Sampler;
				break;
			}
		}
		if (Binding.SamplerSlot != FSamplerHandle::InvalidIndex)
		{
			TextureBinding.Sampler = Renderer.GetSamplerStateCache().GetOrCreate(Sampler);
		}
		Textures.push_back(TextureBinding);
	}

	SortId = InSortId;
	SourceRevision = Command.Revision;
	return true;
}

void FMaterialResource::Release()
{
	Textures.clear();
	ConstantBuffers.clear();
	Constants.clear();
	PipelineState = {};
	SortId = 0;
	SourceRevision = 0;
}

// Material의 Vertex/Pixel Shader Reflection을 하나의 Parameter Layout으로 합친다.
void FMaterialResource::BuildParameterLayout(FMaterialParameterLayout& Layout, FShaderRegistry& ShaderRegistry, const FMaterial& Material)
{
	AppendShaderReflection(Layout, ShaderRegistry.GetReflection(Material.GetVertexShader()));
	AppendShaderReflection(Layout, ShaderRegistry.GetReflection(Material.GetPixelShader()));
}

// Shader Reflection 데이터를 기반으로 Material Parameter Layout을 생성 및 병합한다.
void FMaterialResource::AppendShaderReflection(FMaterialParameterLayout& Layout, const FShaderReflection& Reflection)
{
	static const FName MaterialConstantsName("MaterialConstants");
	for (const FShaderConstantBufferDesc& Buffer : Reflection.ConstantBuffers)
	{
		if (Buffer.Name != MaterialConstantsName)
		{
			continue;
		}

		panicf(Layout.ConstantBufferSize == 0 || Layout.ConstantBufferSize == Buffer.Size,
		       "Vertex/Pixel MaterialConstants 크기가 일치하지 않는다. Existing={}, Incoming={}", Layout.ConstantBufferSize, Buffer.Size);
		Layout.ConstantBufferSize = Buffer.Size;
		Layout.ConstantBuffers.push_back({ Buffer.Stage, Buffer.Slot });
		for (const FShaderParameterDesc& ShaderParameter : Buffer.Parameters)
		{
			const auto Existing = std::find_if(Layout.Parameters.begin(), Layout.Parameters.end(), [&ShaderParameter](const FMaterialParameterDesc& Parameter)
			                                   { return Parameter.Name == ShaderParameter.Name; });
			if (Existing != Layout.Parameters.end())
			{
				panicf(Existing->Offset == ShaderParameter.Offset && Existing->Size == ShaderParameter.Size,
				       "Vertex/Pixel Material Parameter 배치가 일치하지 않는다. Name={}", ShaderParameter.Name.ToString());
				continue;
			}

			panicf(ShaderParameter.BaseType == EShaderParameterBaseType::Float,
			       "Material Parameter는 현재 float Scalar/Vector만 지원한다. Name={}", ShaderParameter.Name.ToString());
			panicf(ShaderParameter.Elements == 0, "Material Parameter 배열은 아직 지원하지 않는다. Name={}, Count={}", ShaderParameter.Name.ToString(), ShaderParameter.Elements);
			FMaterialParameterDesc Parameter;
			Parameter.Name = ShaderParameter.Name;
			Parameter.Offset = ShaderParameter.Offset;
			Parameter.Size = ShaderParameter.Size;
			if (ShaderParameter.Class == EShaderParameterClass::Scalar && ShaderParameter.Columns == 1)
			{
				Parameter.Type = EMaterialParameterType::Scalar;
			}
			else if (ShaderParameter.Class == EShaderParameterClass::Vector && ShaderParameter.Columns >= 2 && ShaderParameter.Columns <= 4)
			{
				Parameter.Type = static_cast<EMaterialParameterType>(static_cast<uint8>(EMaterialParameterType::Vector2) + ShaderParameter.Columns - 2);
			}
			else
			{
				panicf(false, "지원하지 않는 Material Parameter 형태다. Name={}, Rows={}, Columns={}",
				       ShaderParameter.Name.ToString(), ShaderParameter.Rows, ShaderParameter.Columns);
			}
			panicf(Parameter.Size == ShaderParameter.Columns * sizeof(float),
			       "Material Parameter 크기가 Scalar/Vector 형태와 일치하지 않는다. Name={}, Size={}", Parameter.Name.ToString(), Parameter.Size);
			panicf(Parameter.Offset + Parameter.Size <= Buffer.Size,
			       "Material Parameter가 Constant Buffer 범위를 벗어났다. Name={}", Parameter.Name.ToString());
			Layout.Parameters.push_back(std::move(Parameter));
		}
	}

	for (const FShaderResourceBindingDesc& Resource : Reflection.Resources)
	{
		if (Resource.Type != EShaderResourceType::Texture2D && Resource.Type != EShaderResourceType::TextureCube)
		{
			continue;
		}
		panicf(Resource.Count == 1, "Material Texture 배열은 아직 지원하지 않는다. Name={}, Count={}", Resource.Name.ToString(), Resource.Count);

		FMaterialTextureBinding Binding;
		Binding.Name = Resource.Name;
		Binding.Stage = Resource.Stage;
		Binding.TextureSlot = Resource.Slot;
		const auto Sampler = std::find_if(Reflection.Resources.begin(), Reflection.Resources.end(), [&Resource](const FShaderResourceBindingDesc& Candidate)
		{
			return Candidate.Type == EShaderResourceType::Sampler && Candidate.Stage == Resource.Stage && Candidate.Slot == Resource.Slot;
		});
		if (Sampler != Reflection.Resources.end())
		{
			Binding.SamplerSlot = Sampler->Slot;
		}
		Layout.Textures.push_back(std::move(Binding));
	}
}
