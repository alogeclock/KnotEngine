#include "Render/Resource/Material/Material.h"

#include "Core/Assert.h"

#include <algorithm>
#include <utility>

// Material의 Shader 식별자와 Opaque/Masked/Translucent 공통 Pipeline 상태를 초기화한다.
bool FMaterial::Initialize(FShaderKey InVertexShader, FShaderKey InPixelShader, EMaterialBlendMode InBlendMode, EDepthMode InDepthMode, ECullMode InCullMode)
{
	if (InVertexShader.SourcePath.empty() || InVertexShader.EntryPoint.empty() || InVertexShader.Stage != EShaderStage::Vertex ||
	    InPixelShader.SourcePath.empty() || InPixelShader.EntryPoint.empty() || InPixelShader.Stage != EShaderStage::Pixel)
	{
		return false;
	}

	VertexShader = std::move(InVertexShader);
	PixelShader = std::move(InPixelShader);
	BlendMode = InBlendMode;
	DepthMode = InDepthMode;
	CullMode = InCullMode;
	ParameterLayout = {};
	return true;
}

// 두 Shader Reflection의 MaterialConstants 및 Texture Register를 하나의 Material Layout으로 병합한다.
const FMaterialParameterLayout& FMaterial::GetOrCreateParameterLayout(FShaderRegistry& ShaderRegistry) const
{
	if (!ParameterLayout.bInitialized)
	{
		AppendShaderReflection(ParameterLayout, ShaderRegistry.GetReflection(VertexShader));
		AppendShaderReflection(ParameterLayout, ShaderRegistry.GetReflection(PixelShader));
		ParameterLayout.bInitialized = true;
	}
	return ParameterLayout;
}

void FMaterial::AppendShaderReflection(FMaterialParameterLayout& Layout, const FShaderReflection& Reflection)
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
			panicf(ShaderParameter.Elements == 0, "Material Parameter 배열은 아직 지원하지 않는다. Name={}", ShaderParameter.Name.ToString());
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
				Parameter.Type = static_cast<EMaterialParameterType>(
				    static_cast<uint8>(EMaterialParameterType::Vector2) + ShaderParameter.Columns - 2);
			}
			else
			{
				panicf(false, "지원하지 않는 Material Parameter 형태다. Name={}, Rows={}, Columns={}",
				       ShaderParameter.Name.ToString(), ShaderParameter.Rows, ShaderParameter.Columns);
			}
			panicf(Parameter.Size == ShaderParameter.Columns * sizeof(float),
			       "Material Parameter 크기가 Scalar/Vector 형태와 일치하지 않는다. Name={}, Size={}", Parameter.Name.ToString(), Parameter.Size);
			panicf(Parameter.Offset + Parameter.Size <= Buffer.Size, "Material Parameter가 Constant Buffer 범위를 벗어났다. Name={}", Parameter.Name.ToString());
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
		                                  { return Candidate.Type == EShaderResourceType::Sampler && Candidate.Stage == Resource.Stage && Candidate.Slot == Resource.Slot; });
		if (Sampler != Reflection.Resources.end())
		{
			Binding.SamplerSlot = Sampler->Slot;
		}
		Layout.Textures.push_back(std::move(Binding));
	}
}
