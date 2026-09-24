#include "Render/Resource/MaterialResource.h"

#include "Asset/Material/Material.h"
#include "Core/Assert.h"
#include "Render/Renderer.h"
#include "Render/Resource/ResourceCommand.h"
#include "Render/Resource/Mesh/Vertex.h"
#include "Render/Resource/TextureResource.h"

#include <algorithm>
#include <cstring>

// Material 후보를 완성한 뒤 기존 Resource의 내부 상태와 교체한다.
bool FMaterialResource::Initialize(FRenderer& Renderer, const FMaterialResourceCommand& Command, uint32 InSortId, FString* Diagnostics)
{
	FMaterialResource Candidate;
	if (!Candidate.Build(Renderer, Command, InSortId, Diagnostics))
	{
		return false;
	}
	Swap(Candidate);
	return true;
}

// Resource 주소를 유지하면서 Pipeline, 상수 및 Texture 바인딩을 후보와 교환한다.
void FMaterialResource::Swap(FMaterialResource& Other)
{
	using std::swap;
	
	swap(SortId, Other.SortId);
	swap(SourceRevision, Other.SourceRevision);
	swap(PipelineState, Other.PipelineState);
	swap(InstancedPipelineState, Other.InstancedPipelineState);
	swap(PipelineStateDesc, Other.PipelineStateDesc);
	swap(InstancedPipelineStateDesc, Other.InstancedPipelineStateDesc);
	swap(Constants, Other.Constants);
	swap(ConstantBuffers, Other.ConstantBuffers);
	swap(Textures, Other.Textures);
}

// Shader 교체로 준비된 PSO만 반영하고 Material 값과 바인딩은 유지한다.
void FMaterialResource::SetPipelineState(bool bInstanced, FPipelineStateDesc Desc, FPipelineStateHandle Handle)
{
	check(Handle.IsValid());
	if (bInstanced)
	{
		InstancedPipelineStateDesc = std::move(Desc);
		InstancedPipelineState = Handle;
	}
	else
	{
		PipelineStateDesc = std::move(Desc);
		PipelineState = Handle;
	}
}

// Material 명령과 Shader Reflection으로 Pipeline, 상수 및 Texture 바인딩을 준비한다.
bool FMaterialResource::Build(FRenderer& Renderer, const FMaterialResourceCommand& Command, uint32 InSortId, FString* Diagnostics)
{
	if (Command.Revision == 0)
	{
		return false;
	}

	static const FShaderKey DefaultVertexShader{ "/Engine/Shader/StaticMesh.hlsl", "MainVS", EShaderStage::Vertex };
	static const FShaderKey InstancedVertexShader{ "/Engine/Shader/StaticMesh.hlsl", "InstancedVS", EShaderStage::Vertex };
	static const FShaderKey DefaultPixelShader{ "/Engine/Shader/StaticMesh.hlsl", "OpaquePS", EShaderStage::Pixel };
	FMaterial DefaultMaterial;
	verify(DefaultMaterial.Initialize(DefaultVertexShader, DefaultPixelShader));

	const bool bHasMaterialAsset = Command.Material.IsValid();
	const FMaterial& Material = bHasMaterialAsset ? Command.Material : DefaultMaterial;
	FShaderRegistry& ShaderRegistry = Renderer.GetShaderRegistry();

	FPipelineStateDesc SurfaceDesc;
	SurfaceDesc.VertexShader = ShaderRegistry.GetOrCreate(Material.GetVertexShader());
	SurfaceDesc.PixelShader = ShaderRegistry.GetOrCreate(Material.GetPixelShader());
	SurfaceDesc.VertexLayout = FStaticMeshVertex::GetVertexLayout();
	SurfaceDesc.RasterizerState.CullMode = Material.GetCullMode();
	SurfaceDesc.DepthMode = Material.GetDepthMode();
	if (Material.GetBlendMode() == EMaterialBlendMode::Translucent)
	{
		FRenderTargetBlendDesc& Blend = SurfaceDesc.BlendState.RenderTarget;
		Blend.bBlendEnabled = true;
		Blend.SourceColorBlend = EBlendFactor::SourceAlpha;
		Blend.DestinationColorBlend = EBlendFactor::InverseSourceAlpha;
		Blend.SourceAlphaBlend = EBlendFactor::One;
		Blend.DestinationAlphaBlend = EBlendFactor::InverseSourceAlpha;
	}
	PipelineStateDesc = SurfaceDesc;
	FString LocalDiagnostics;
	FString& Error = Diagnostics ? *Diagnostics : LocalDiagnostics;
	if (!Renderer.GetPipelineStateCache().TryGetOrCreate(PipelineStateDesc, PipelineState, Error))
	{
		return false;
	}
	if (Material.GetVertexShader() == DefaultVertexShader && Material.GetBlendMode() != EMaterialBlendMode::Translucent)
	{
		static const FVertexLayout InstancedVertexLayout = []
		{
			FVertexLayout Layout = FStaticMeshVertex::GetVertexLayout();
			const FVertexLayout& InstanceLayout = FStaticMeshInstance::GetVertexLayout();
			Layout.Elements.insert(Layout.Elements.end(), InstanceLayout.Elements.begin(), InstanceLayout.Elements.end());
			return Layout;
		}();
		InstancedPipelineStateDesc = SurfaceDesc;
		InstancedPipelineStateDesc.VertexShader = ShaderRegistry.GetOrCreate(InstancedVertexShader);
		InstancedPipelineStateDesc.VertexLayout = InstancedVertexLayout;
		if (!Renderer.GetPipelineStateCache().TryGetOrCreate(InstancedPipelineStateDesc, InstancedPipelineState, Error))
		{
			return false;
		}
	}

	FMaterialParameterLayout Layout;
	if (!BuildParameterLayout(Layout, ShaderRegistry, Material, Diagnostics))
	{
		return false;
	}
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
	InstancedPipelineState = {};
	PipelineStateDesc = {};
	InstancedPipelineStateDesc = {};
	SortId = 0;
	SourceRevision = 0;
}

// Material의 Vertex/Pixel Shader Reflection을 하나의 Parameter Layout으로 합친다.
bool FMaterialResource::BuildParameterLayout(FMaterialParameterLayout& Layout, FShaderRegistry& ShaderRegistry, const FMaterial& Material, FString* Diagnostics)
{
	return AppendShaderReflection(Layout, ShaderRegistry.GetReflection(Material.GetVertexShader()), Diagnostics) &&
		AppendShaderReflection(Layout, ShaderRegistry.GetReflection(Material.GetPixelShader()), Diagnostics);
}

// Shader Reflection 데이터를 기반으로 Material Parameter Layout을 생성 및 병합한다.
bool FMaterialResource::AppendShaderReflection(FMaterialParameterLayout& Layout, const FShaderReflection& Reflection, FString* Diagnostics)
{
	const auto Fail = [Diagnostics](const FString& Message)
	{
		if (Diagnostics)
		{
			*Diagnostics = Message;
		}
		return false;
	};
	static const FName MaterialConstantsName("MaterialConstants");
	for (const FShaderConstantBufferDesc& Buffer : Reflection.ConstantBuffers)
	{
		if (Buffer.Name != MaterialConstantsName)
		{
			continue;
		}

		if (Layout.ConstantBufferSize != 0 && Layout.ConstantBufferSize != Buffer.Size)
		{
			return Fail("Vertex/Pixel MaterialConstants 크기가 일치하지 않는다.");
		}
		Layout.ConstantBufferSize = Buffer.Size;
		Layout.ConstantBuffers.push_back({ Buffer.Stage, Buffer.Slot });
		for (const FShaderParameterDesc& ShaderParameter : Buffer.Parameters)
		{
			const auto Existing = std::find_if(Layout.Parameters.begin(), Layout.Parameters.end(), [&ShaderParameter](const FMaterialParameterDesc& Parameter)
			                                   { return Parameter.Name == ShaderParameter.Name; });
			if (Existing != Layout.Parameters.end())
			{
				if (Existing->Offset != ShaderParameter.Offset || Existing->Size != ShaderParameter.Size)
				{
					return Fail("Vertex/Pixel Material Parameter 배치가 일치하지 않는다: " + ShaderParameter.Name.ToString());
				}
				continue;
			}

			if (ShaderParameter.BaseType != EShaderParameterBaseType::Float || ShaderParameter.Elements != 0)
			{
				return Fail("지원하지 않는 Material Parameter 타입 또는 배열: " + ShaderParameter.Name.ToString());
			}
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
				return Fail("지원하지 않는 Material Parameter 형태: " + ShaderParameter.Name.ToString());
			}
			if (Parameter.Size != ShaderParameter.Columns * sizeof(float) || Parameter.Offset + Parameter.Size > Buffer.Size)
			{
				return Fail("Material Parameter 크기 또는 범위가 유효하지 않다: " + Parameter.Name.ToString());
			}
			Layout.Parameters.push_back(std::move(Parameter));
		}
	}

	for (const FShaderResourceBindingDesc& Resource : Reflection.Resources)
	{
		if (Resource.Type != EShaderResourceType::Texture2D && Resource.Type != EShaderResourceType::TextureCube)
		{
			continue;
		}
		if (Resource.Count != 1)
		{
			return Fail("Material Texture 배열은 아직 지원하지 않는다: " + Resource.Name.ToString());
		}

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
	return true;
}
