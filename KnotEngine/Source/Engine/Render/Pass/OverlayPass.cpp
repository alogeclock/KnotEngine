#include "Render/Pass/OverlayPass.h"

#include "Core/Assert.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Render/Renderer.h"
#include "Render/Resource/MeshResources.h"
#include "Render/Resource/MeshTypes.h"
#include "Render/Resource/VertexTypes.h"
#include "Render/RHI/RenderDevice.h"
#include "Render/Scene/SceneView.h"
#include "Source/Resource/resource.h"

uint32 FOverlayPass::AddPass(
	FRenderGraph& Graph,
	URenderer& Renderer,
	const FSceneView& View,
	const FShowFlags& ShowFlags,
	std::span<const FPrimitiveSceneProxy* const> VisiblePrimitives)
{
	IRenderDevice* RenderDevice = &Renderer.GetRenderDevice();
	const FCommandListHandle CommandList = Renderer.GetCommandList();
	check(CommandList.IsValid());

	FShaderRegistry& ShaderRegistry = Renderer.GetShaderRegistry();
	FPipelineStateCache& PipelineStateCache = Renderer.GetPipelineStateCache();
	FPassParameters Parameters;
	if (ShowFlags.bGrid)
	{
		const FShaderHandle VertexShader = ShaderRegistry.GetOrCreate({ IDR_GRID_SHADER, "Grid.hlsl", "VS", EShaderStage::Vertex });
		const FShaderHandle PixelShader = ShaderRegistry.GetOrCreate({ IDR_GRID_SHADER, "Grid.hlsl", "PS", EShaderStage::Pixel });
		FPipelineStateDesc PipelineStateDesc;
		PipelineStateDesc.VertexShader = VertexShader;
		PipelineStateDesc.PixelShader = PixelShader;
		PipelineStateDesc.bDepthTestEnabled = true;
		PipelineStateDesc.bDepthWriteEnabled = false;
		PipelineStateDesc.BlendState.RenderTarget.bBlendEnabled = true;
		PipelineStateDesc.BlendState.RenderTarget.SourceColorBlend = EBlendFactor::SourceAlpha;
		PipelineStateDesc.BlendState.RenderTarget.DestinationColorBlend = EBlendFactor::InverseSourceAlpha;
		PipelineStateDesc.BlendState.RenderTarget.SourceAlphaBlend = EBlendFactor::One;
		PipelineStateDesc.BlendState.RenderTarget.DestinationAlphaBlend = EBlendFactor::InverseSourceAlpha;
		PipelineStateDesc.RasterizerState.CullMode = ECullMode::None;
		Parameters.GridPipeline = PipelineStateCache.GetOrCreate(PipelineStateDesc);
		Parameters.GridConstants = {
			5.0f,
			10.0f,
			0.0f,
			0.0f,
			FVector4(0.22f, 0.24f, 0.28f, 0.45f),
			FVector4(0.38f, 0.41f, 0.46f, 0.65f),
		};
	}

	if (ShowFlags.bAxis)
	{
		const FShaderHandle VertexShader = ShaderRegistry.GetOrCreate({ IDR_AXIS_SHADER, "Axis.hlsl", "VS", EShaderStage::Vertex });
		const FShaderHandle PixelShader = ShaderRegistry.GetOrCreate({ IDR_AXIS_SHADER, "Axis.hlsl", "PS", EShaderStage::Pixel });
		FPipelineStateDesc PipelineStateDesc;
		PipelineStateDesc.VertexShader = VertexShader;
		PipelineStateDesc.PixelShader = PixelShader;
		PipelineStateDesc.PrimitiveTopology = EPrimitiveTopology::LineList;
		PipelineStateDesc.bDepthTestEnabled = true;
		PipelineStateDesc.bDepthWriteEnabled = false;
		PipelineStateDesc.BlendState.RenderTarget.bBlendEnabled = true;
		PipelineStateDesc.BlendState.RenderTarget.SourceColorBlend = EBlendFactor::SourceAlpha;
		PipelineStateDesc.BlendState.RenderTarget.DestinationColorBlend = EBlendFactor::InverseSourceAlpha;
		PipelineStateDesc.BlendState.RenderTarget.SourceAlphaBlend = EBlendFactor::One;
		PipelineStateDesc.BlendState.RenderTarget.DestinationAlphaBlend = EBlendFactor::InverseSourceAlpha;
		PipelineStateDesc.RasterizerState.CullMode = ECullMode::None;
		PipelineStateDesc.RasterizerState.bAntialiasedLineEnabled = true;
		Parameters.AxisPipeline = PipelineStateCache.GetOrCreate(PipelineStateDesc);
	}

	if (ShowFlags.bBounds)
	{
		FGeometryMesh& BoundsMesh = Renderer.GetDebugBoundsMesh();
		panicf(BoundsMesh.InitResources(*RenderDevice), "Bounds Mesh의 GPU Buffer 생성에 실패했다.");
		Parameters.BoundsMeshBuffer = &BoundsMesh.GetMeshBuffer();

		const FShaderHandle VertexShader = ShaderRegistry.GetOrCreate({ IDR_GEOMETRY_MESH_SHADER, "GeometryMesh.hlsl", "VS", EShaderStage::Vertex });
		const FShaderHandle PixelShader = ShaderRegistry.GetOrCreate({ IDR_GEOMETRY_MESH_SHADER, "GeometryMesh.hlsl", "PS", EShaderStage::Pixel });
		FPipelineStateDesc PipelineStateDesc;
		PipelineStateDesc.VertexShader = VertexShader;
		PipelineStateDesc.PixelShader = PixelShader;
		PipelineStateDesc.VertexLayout = FGeometryVertex::GetVertexLayout();
		PipelineStateDesc.PrimitiveTopology = EPrimitiveTopology::LineList;
		PipelineStateDesc.bDepthTestEnabled = false;
		PipelineStateDesc.bDepthWriteEnabled = false;
		PipelineStateDesc.RasterizerState.CullMode = ECullMode::None;
		PipelineStateDesc.RasterizerState.bAntialiasedLineEnabled = true;
		Parameters.BoundsPipeline = PipelineStateCache.GetOrCreate(PipelineStateDesc);

		Parameters.BoundsCommands.reserve(VisiblePrimitives.size());
		for (const FPrimitiveSceneProxy* Primitive : VisiblePrimitives)
		{
			const FVector Center = Primitive->WorldBounds.GetCenter();
			const FVector Extent = Primitive->WorldBounds.GetExtent();
			Parameters.BoundsCommands.push_back({ FMatrix::MakeScale(Extent) * FMatrix::MakeTranslation(Center) });
		}
	}

	const FViewConstants ViewConstants = {
		View.ViewProjectionMatrix,
		View.ViewProjectionMatrix.GetInverse(),
		View.ViewOrigin,
		View.FarClip,
	};
	const FRenderViewport Viewport = View.Viewport;
	return Graph.AddPass("Overlay", [RenderDevice, CommandList, Viewport, ViewConstants, Parameters = std::move(Parameters)]()
	{
		ExecutePass(*RenderDevice, CommandList, Viewport, ViewConstants, Parameters);
	});
}

void FOverlayPass::ExecutePass(
	IRenderDevice& RenderDevice,
	FCommandListHandle CommandList,
	const FRenderViewport& Viewport,
	const FViewConstants& ViewConstants,
	const FPassParameters& Parameters)
{
	RenderDevice.SetViewport(CommandList, Viewport);
	if (Parameters.GridPipeline.IsValid())
	{
		DrawGrid(RenderDevice, CommandList, ViewConstants, Parameters);
	}
	if (Parameters.AxisPipeline.IsValid())
	{
		DrawAxis(RenderDevice, CommandList, ViewConstants, Parameters);
	}
	if (Parameters.BoundsPipeline.IsValid())
	{
		DrawBounds(RenderDevice, CommandList, ViewConstants, Parameters);
	}
}

void FOverlayPass::DrawGrid(
	IRenderDevice& RenderDevice,
	FCommandListHandle CommandList,
	const FViewConstants& ViewConstants,
	const FPassParameters& Parameters)
{
	RenderDevice.SetPipelineState(CommandList, Parameters.GridPipeline);
	const auto* ViewBytes = reinterpret_cast<const uint8*>(&ViewConstants);
	RenderDevice.SetConstantData(CommandList, EShaderStage::Pixel, ViewConstantsSlot, std::span<const uint8>(ViewBytes, sizeof(ViewConstants)));
	const auto* GridBytes = reinterpret_cast<const uint8*>(&Parameters.GridConstants);
	RenderDevice.SetConstantData(CommandList, EShaderStage::Pixel, PassConstantsSlot, std::span<const uint8>(GridBytes, sizeof(Parameters.GridConstants)));
	RenderDevice.Draw(CommandList, 3);
}

void FOverlayPass::DrawAxis(
	IRenderDevice& RenderDevice,
	FCommandListHandle CommandList,
	const FViewConstants& ViewConstants,
	const FPassParameters& Parameters)
{
	RenderDevice.SetPipelineState(CommandList, Parameters.AxisPipeline);
	const auto* ViewBytes = reinterpret_cast<const uint8*>(&ViewConstants);
	RenderDevice.SetConstantData(CommandList, EShaderStage::Vertex, ViewConstantsSlot, std::span<const uint8>(ViewBytes, sizeof(ViewConstants)));
	RenderDevice.SetConstantData(CommandList, EShaderStage::Pixel, ViewConstantsSlot, std::span<const uint8>(ViewBytes, sizeof(ViewConstants)));
	RenderDevice.Draw(CommandList, 6);
}

void FOverlayPass::DrawBounds(
	IRenderDevice& RenderDevice,
	FCommandListHandle CommandList,
	const FViewConstants& ViewConstants,
	const FPassParameters& Parameters)
{
	check(Parameters.BoundsMeshBuffer);
	const FMeshBuffer& MeshBuffer = *Parameters.BoundsMeshBuffer;
	checkf(MeshBuffer.IsValid(), "유효하지 않은 FMeshBuffer가 Overlay Pass에 전달되었다.");
	checkf(MeshBuffer.GetLayout() == FGeometryVertex::GetVertexLayout(), "Bounds Pipeline State와 호환되지 않는 Vertex Layout이다.");

	RenderDevice.SetPipelineState(CommandList, Parameters.BoundsPipeline);
	const auto* ViewBytes = reinterpret_cast<const uint8*>(&ViewConstants);
	RenderDevice.SetConstantData(CommandList, EShaderStage::Vertex, ViewConstantsSlot, std::span<const uint8>(ViewBytes, sizeof(ViewConstants)));
	RenderDevice.SetVertexBuffer(CommandList, MeshBuffer.GetVertexBuffer().GetHandle(), MeshBuffer.GetStride());
	RenderDevice.SetIndexBuffer(CommandList, MeshBuffer.GetIndexBuffer().GetHandle(), EIndexFormat::UInt32);

	for (const FBoundsDrawCommand& Command : Parameters.BoundsCommands)
	{
		const FDrawConstants DrawConstants = { Command.Model };
		const auto* DrawBytes = reinterpret_cast<const uint8*>(&DrawConstants);
		RenderDevice.SetConstantData(CommandList, EShaderStage::Vertex, DrawConstantsSlot, std::span<const uint8>(DrawBytes, sizeof(DrawConstants)));
		RenderDevice.DrawIndexed(CommandList, MeshBuffer.GetIndexCount());
	}
}
