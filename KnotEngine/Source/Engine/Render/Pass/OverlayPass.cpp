#include "Render/Pass/OverlayPass.h"

#include "Core/Assert.h"
#include "Render/Renderer.h"
#include "Render/RHI/RenderDevice.h"
#include "Render/Scene/SceneView.h"

uint32 FOverlayPass::AddPass(
	FRenderGraph& Graph,
	URenderer& Renderer,
	const FSceneView& View,
	const FShowFlags& ShowFlags,
	FTextureHandle ColorTarget,
	FTextureHandle DepthTarget,
	FTextureHandle SelectionDepth,
	bool bHasSelection)
{
	check(ColorTarget.IsValid() && DepthTarget.IsValid() && SelectionDepth.IsValid());
	IRenderDevice* RenderDevice = &Renderer.GetRenderDevice();
	const FCommandListHandle CommandList = Renderer.GetCommandList();
	check(CommandList.IsValid());

	FShaderRegistry& ShaderRegistry = Renderer.GetShaderRegistry();
	FPipelineStateCache& PipelineStateCache = Renderer.GetPipelineStateCache();
	FPassParameters Parameters;
	Parameters.OverlayConstants.HasSelection = bHasSelection ? 1u : 0u;
	if (ShowFlags.bGrid)
	{
		const FShaderHandle VertexShader = ShaderRegistry.GetOrCreate({ "/Engine/Shader/Grid.hlsl", "VS", EShaderStage::Vertex });
		const FShaderHandle PixelShader = ShaderRegistry.GetOrCreate({ "/Engine/Shader/Grid.hlsl", "PS", EShaderStage::Pixel });
		FPipelineStateDesc PipelineStateDesc;
		PipelineStateDesc.VertexShader = VertexShader;
		PipelineStateDesc.PixelShader = PixelShader;
		PipelineStateDesc.DepthMode = EDepthMode::ReadOnly;
		PipelineStateDesc.BlendState.RenderTarget.bBlendEnabled = true;
		PipelineStateDesc.BlendState.RenderTarget.SourceColorBlend = EBlendFactor::SourceAlpha;
		PipelineStateDesc.BlendState.RenderTarget.DestinationColorBlend = EBlendFactor::InverseSourceAlpha;
		PipelineStateDesc.BlendState.RenderTarget.SourceAlphaBlend = EBlendFactor::One;
		PipelineStateDesc.BlendState.RenderTarget.DestinationAlphaBlend = EBlendFactor::InverseSourceAlpha;
		PipelineStateDesc.RasterizerState.CullMode = ECullMode::None;
		Parameters.GridPipeline = PipelineStateCache.GetOrCreate(PipelineStateDesc);
		Parameters.OverlayConstants.GridSpacing = 20.0f;
		Parameters.OverlayConstants.MajorGridInterval = 5.0f;
		Parameters.OverlayConstants.MinorColor = FVector4(0.30f, 0.33f, 0.38f, 0.3f);
		Parameters.OverlayConstants.MajorColor = FVector4(0.42f, 0.46f, 0.52f, 0.5f);
	}

	if (ShowFlags.bAxis)
	{
		const FShaderHandle VertexShader = ShaderRegistry.GetOrCreate({ "/Engine/Shader/Axis.hlsl", "VS", EShaderStage::Vertex });
		const FShaderHandle PixelShader = ShaderRegistry.GetOrCreate({ "/Engine/Shader/Axis.hlsl", "PS", EShaderStage::Pixel });
		FPipelineStateDesc PipelineStateDesc;
		PipelineStateDesc.VertexShader = VertexShader;
		PipelineStateDesc.PixelShader = PixelShader;
		PipelineStateDesc.PrimitiveTopology = EPrimitiveTopology::LineList;
		PipelineStateDesc.DepthMode = EDepthMode::ReadOnly;
		PipelineStateDesc.BlendState.RenderTarget.bBlendEnabled = true;
		PipelineStateDesc.BlendState.RenderTarget.SourceColorBlend = EBlendFactor::SourceAlpha;
		PipelineStateDesc.BlendState.RenderTarget.DestinationColorBlend = EBlendFactor::InverseSourceAlpha;
		PipelineStateDesc.BlendState.RenderTarget.SourceAlphaBlend = EBlendFactor::One;
		PipelineStateDesc.BlendState.RenderTarget.DestinationAlphaBlend = EBlendFactor::InverseSourceAlpha;
		PipelineStateDesc.RasterizerState.CullMode = ECullMode::None;
		PipelineStateDesc.RasterizerState.bAntialiasedLineEnabled = true;
		Parameters.AxisPipeline = PipelineStateCache.GetOrCreate(PipelineStateDesc);
	}

	const FViewConstants ViewConstants = {
		View.ViewProjectionMatrix,
		View.ViewProjectionMatrix.GetInverse(),
		View.ViewOrigin,
		View.FarClip,
	};
	const FRenderViewport Viewport = View.Viewport;
	return Graph.AddPass("Overlay", [RenderDevice, CommandList, ColorTarget, DepthTarget, SelectionDepth, Viewport, ViewConstants,
		Parameters = std::move(Parameters)]()
	{
		ExecutePass(*RenderDevice, CommandList, ColorTarget, DepthTarget, SelectionDepth, Viewport, ViewConstants, Parameters);
	});
}

void FOverlayPass::ExecutePass(
	IRenderDevice& RenderDevice,
	FCommandListHandle CommandList,
	FTextureHandle ColorTarget,
	FTextureHandle DepthTarget,
	FTextureHandle SelectionDepth,
	const FRenderViewport& Viewport,
	const FViewConstants& ViewConstants,
	const FPassParameters& Parameters)
{
	RenderDevice.SetRenderTargets(CommandList, ColorTarget, DepthTarget);
	RenderDevice.SetViewport(CommandList, Viewport);
	RenderDevice.SetTexture(CommandList, EShaderStage::Pixel, 0, SelectionDepth);
	const auto* OverlayBytes = reinterpret_cast<const uint8*>(&Parameters.OverlayConstants);
	RenderDevice.SetConstantData(
		CommandList,
		EShaderStage::Pixel,
		OverlayConstantsSlot,
		std::span<const uint8>(OverlayBytes, sizeof(Parameters.OverlayConstants)));
	if (Parameters.GridPipeline.IsValid())
	{
		DrawGrid(RenderDevice, CommandList, ViewConstants, Parameters);
	}
	if (Parameters.AxisPipeline.IsValid())
	{
		DrawAxis(RenderDevice, CommandList, ViewConstants, Parameters);
	}
	RenderDevice.SetTexture(CommandList, EShaderStage::Pixel, 0, {});
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
