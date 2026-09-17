#include "Render/Pass/OpaquePass.h"

#include "Asset/Material/MaterialInterface.h"
#include "Asset/Texture/Texture.h"
#include "Core/Assert.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Render/Renderer.h"
#include "Render/Resource/Mesh/MeshBuffer.h"
#include "Render/Resource/Mesh/Mesh.h"
#include "Render/Resource/Material/Material.h"
#include "Render/Resource/Texture.h"
#include "Render/Resource/Mesh/Vertex.h"
#include "Render/RHI/RenderDevice.h"
#include "Render/Scene/SceneView.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>

uint32 FOpaquePass::AddPass(FRenderGraph& Graph, URenderer& Renderer, const FSceneView& View, std::span<const FPrimitiveSceneProxy* const> VisiblePrimitives)
{
	IRenderDevice* RenderDevice = &Renderer.GetRenderDevice();

	const FCommandListHandle CommandList = Renderer.GetCommandList();
	check(CommandList.IsValid());

	FShaderRegistry& ShaderRegistry = Renderer.GetShaderRegistry();
	FPipelineStateCache& PipelineStateCache = Renderer.GetPipelineStateCache();
	FSamplerStateCache& SamplerStateCache = Renderer.GetSamplerStateCache();

	static const FShaderKey DefaultVertexShader{ "/Engine/Shader/StaticMesh.hlsl", "MainVS", EShaderStage::Vertex };
	static const FShaderKey DefaultPixelShader{ "/Engine/Shader/StaticMesh.hlsl", "OpaquePS", EShaderStage::Pixel };
	FMaterial DefaultMaterial;
	verify(DefaultMaterial.Initialize(DefaultVertexShader, DefaultPixelShader));

	TArray<FMeshDrawCommand> OpaqueCommands;
	OpaqueCommands.reserve(VisiblePrimitives.size());

	for (const FPrimitiveSceneProxy* Primitive : VisiblePrimitives)
	{
		const auto& StaticMeshProxy = static_cast<const FStaticMeshSceneProxy&>(*Primitive);
		check(StaticMeshProxy.Mesh);
		panicf(StaticMeshProxy.Mesh->InitResources(*RenderDevice), "Static Mesh의 GPU Buffer 생성에 실패했다.");
		const FStaticMeshLOD& LOD = StaticMeshProxy.Mesh->GetLOD(0);
		const FMeshBuffer* MeshBuffer = &LOD.GetMeshBuffer();
		const float Depth = View.ViewMatrix.TransformPosition(Primitive->WorldBounds.GetCenter()).Z;
		check(!std::isnan(Depth) && !std::isinf(Depth));
		const uint32 SortKey = std::bit_cast<uint32>(std::max(0.0f, Depth));

		for (const FStaticMeshSection& Section : LOD.GetSections())
		{
			const UMaterialInterface* MaterialInterface = StaticMeshProxy.GetMaterial(Section.MaterialIndex);
			const FMaterial* Material = MaterialInterface ? MaterialInterface->GetMaterial() : nullptr;
			const bool bHasMaterialAsset = Material && Material->IsValid();
			const FMaterial& MaterialDefinition = bHasMaterialAsset ? *Material : DefaultMaterial;

			FPipelineStateDesc PipelineStateDesc;
			PipelineStateDesc.VertexShader = ShaderRegistry.GetOrCreate(MaterialDefinition.GetVertexShader());
			PipelineStateDesc.PixelShader = ShaderRegistry.GetOrCreate(MaterialDefinition.GetPixelShader());
			PipelineStateDesc.VertexLayout = FStaticMeshVertex::GetVertexLayout();
			PipelineStateDesc.RasterizerState.CullMode = MaterialDefinition.GetCullMode();
			PipelineStateDesc.bDepthTestEnabled = MaterialDefinition.GetDepthMode() != EMaterialDepthMode::Disabled;
			PipelineStateDesc.bDepthWriteEnabled = MaterialDefinition.GetDepthMode() == EMaterialDepthMode::ReadWrite;

			if (MaterialDefinition.GetBlendMode() == EMaterialBlendMode::Translucent)
			{
				FRenderTargetBlendDesc& Blend = PipelineStateDesc.BlendState.RenderTarget;
				Blend.bBlendEnabled = true;
				Blend.SourceColorBlend = EBlendFactor::SourceAlpha;
				Blend.DestinationColorBlend = EBlendFactor::InverseSourceAlpha;
				Blend.SourceAlphaBlend = EBlendFactor::One;
				Blend.DestinationAlphaBlend = EBlendFactor::InverseSourceAlpha;
			}

			FMeshDrawCommand Command;
			Command.Primitive = Primitive;
			Command.MeshBuffer = MeshBuffer;
			Command.FirstIndex = Section.FirstIndex;
			Command.IndexCount = Section.IndexCount;
			Command.PipelineState = PipelineStateCache.GetOrCreate(PipelineStateDesc);
			Command.SortKey = SortKey;
			const FMaterialParameterLayout& Layout = MaterialDefinition.GetOrCreateParameterLayout(ShaderRegistry);
			Command.MaterialConstantBuffers = Layout.ConstantBuffers;

			if (bHasMaterialAsset)
			{
				MaterialInterface->PackMaterialConstants(Layout, Command.MaterialConstants);
			}
			else // Default Magenta Material
			{
				static const FName BaseColorName("BaseColor");
				Command.MaterialConstants.assign(Layout.ConstantBufferSize, 0);
				for (const FMaterialParameterDesc& Parameter : Layout.Parameters)
				{
					if (Parameter.Name == BaseColorName && Parameter.Type == EMaterialParameterType::Vector4)
					{
						static const FVector4 DefaultBaseColor(1.0f, 0.0f, 1.0f, 1.0f);
						std::memcpy(Command.MaterialConstants.data() + Parameter.Offset, DefaultBaseColor.Data, sizeof(DefaultBaseColor));
					}
				}
			}

			for (const FMaterialTextureBinding& Binding : Layout.Textures)
			{
				FMeshDrawCommand::FTextureBinding TextureBinding;
				TextureBinding.Stage = Binding.Stage;
				TextureBinding.TextureSlot = Binding.TextureSlot;
				TextureBinding.SamplerSlot = Binding.SamplerSlot;
				TextureBinding.Texture = Renderer.GetDefaultTexture();
				FSamplerDesc Sampler;
				if (bHasMaterialAsset)
				{
					if (const FTextureMaterialParameter* Parameter = MaterialInterface->FindTextureParameter(Binding.Name); Parameter && Parameter->Texture)
					{
						panicf(Parameter->Texture->InitResources(*RenderDevice), "Material Texture의 GPU Resource 생성에 실패했다. Name={}", Binding.Name.ToString());
						TextureBinding.Texture = Parameter->Texture->GetResource()->GetHandle();
						Sampler = Parameter->Sampler;
					}
				}
				if (Binding.SamplerSlot != FSamplerHandle::InvalidIndex)
				{
					TextureBinding.Sampler = SamplerStateCache.GetOrCreate(Sampler);
				}
				Command.Textures.push_back(TextureBinding);
			}
			OpaqueCommands.push_back(std::move(Command));
		}
	}

	std::stable_sort(OpaqueCommands.begin(), OpaqueCommands.end(), [](const FMeshDrawCommand& Left, const FMeshDrawCommand& Right)
	{
		return Left.SortKey < Right.SortKey;
	});

	const FViewConstants ViewConstants = {
		View.ViewProjectionMatrix,
		View.ViewProjectionMatrix.GetInverse(),
		View.ViewOrigin,
		View.FarClip,
	};

	const FRenderViewport Viewport = View.Viewport;

	return Graph.AddPass("Opaque", [RenderDevice, CommandList, Viewport, ViewConstants, OpaqueCommands = std::move(OpaqueCommands)]()
	{
		ExecutePass(*RenderDevice, CommandList, Viewport, ViewConstants, OpaqueCommands);
	});
}

void FOpaquePass::ExecutePass(
	IRenderDevice& RenderDevice,
	FCommandListHandle CommandList,
	const FRenderViewport& Viewport,
	const FViewConstants& ViewConstants,
	const TArray<FMeshDrawCommand>& OpaqueCommands)
{
	RenderDevice.SetViewport(CommandList, Viewport);
	const auto* ViewBytes = reinterpret_cast<const uint8*>(&ViewConstants);

	for (const FMeshDrawCommand& Command : OpaqueCommands)
	{
		RenderDevice.SetPipelineState(CommandList, Command.PipelineState);
		RenderDevice.SetConstantData(CommandList, EShaderStage::Vertex, ViewConstantsSlot, std::span<const uint8>(ViewBytes, sizeof(ViewConstants)));
		if (!Command.MaterialConstants.empty())
		{
			for (const FMaterialConstantBufferBinding& Binding : Command.MaterialConstantBuffers)
			{
				RenderDevice.SetConstantData(CommandList, Binding.Stage, Binding.Slot, Command.MaterialConstants);
			}
		}
		for (const FMeshDrawCommand::FTextureBinding& Binding : Command.Textures)
		{
			RenderDevice.SetTexture(CommandList, Binding.Stage, Binding.TextureSlot, Binding.Texture);
			if (Binding.SamplerSlot != FSamplerHandle::InvalidIndex)
			{
				RenderDevice.SetSampler(CommandList, Binding.Stage, Binding.SamplerSlot, Binding.Sampler);
			}
		}

		const FMeshBuffer& MeshBuffer = *Command.MeshBuffer;
		checkf(MeshBuffer.IsValid(), "유효하지 않은 FMeshBuffer가 Opaque Pass에 전달되었다.");
		checkf(MeshBuffer.GetLayout() == FStaticMeshVertex::GetVertexLayout(), "Opaque Pipeline State와 호환되지 않는 Vertex Layout이다.");

		const FDrawConstants DrawConstants = { Command.Primitive->WorldMatrix };
		const auto* DrawBytes = reinterpret_cast<const uint8*>(&DrawConstants);
		RenderDevice.SetConstantData(CommandList, EShaderStage::Vertex, DrawConstantsSlot, std::span<const uint8>(DrawBytes, sizeof(DrawConstants)));
		RenderDevice.SetVertexBuffer(CommandList, MeshBuffer.GetVertexBuffer().GetHandle(), MeshBuffer.GetStride());
		if (MeshBuffer.GetIndexCount() > 0)
		{
			RenderDevice.SetIndexBuffer(CommandList, MeshBuffer.GetIndexBuffer().GetHandle(), EIndexFormat::UInt32);
			RenderDevice.DrawIndexed(CommandList, Command.IndexCount, Command.FirstIndex);
		}
		else
		{
			RenderDevice.Draw(CommandList, Command.IndexCount, Command.FirstIndex);
		}
	}
}
