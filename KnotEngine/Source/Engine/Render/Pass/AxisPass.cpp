#include "Render/Pass/AxisPass.h"

#include "Core/Assert.h"
#include "Render/Renderer.h"
#include "Render/RHI/RenderDevice.h"
#include "Render/Scene/SceneView.h"
#include "Source/Resource/resource.h"

uint32 FAxisPass::AddPass(FRenderGraph& Graph, URenderer& Renderer, const FSceneView& View)
{
	IRenderDevice* RenderDevice = &Renderer.GetRenderDevice();
	const FCommandListHandle CommandList = Renderer.GetCommandList();
	check(CommandList.IsValid());

	check(GShaderRegistry && GPipelineStateCache);
	const FShaderHandle VertexShader = GShaderRegistry->GetOrCreate({ IDR_AXIS_SHADER, "Axis.hlsl", "VS", EShaderStage::Vertex });
	const FShaderHandle PixelShader = GShaderRegistry->GetOrCreate({ IDR_AXIS_SHADER, "Axis.hlsl", "PS", EShaderStage::Pixel });
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
	const FPipelineStateHandle PipelineState = GPipelineStateCache->GetOrCreate(PipelineStateDesc);

	const FViewConstants ViewConstants = {
		View.ViewProjectionMatrix,
		View.ViewProjectionMatrix.GetInverse(),
		View.ViewOrigin,
		View.FarClip,
	};
	const FRenderViewport Viewport = View.Viewport;

	return Graph.AddPass("Axis", [RenderDevice, CommandList, PipelineState, Viewport, ViewConstants]()
	{
		ExecutePass(*RenderDevice, CommandList, PipelineState, Viewport, ViewConstants);
	});
}

void FAxisPass::ExecutePass(
	IRenderDevice& RenderDevice,
	FCommandListHandle CommandList,
	FPipelineStateHandle PipelineState,
	const FRenderViewport& Viewport,
	const FViewConstants& ViewConstants)
{
	RenderDevice.SetViewport(CommandList, Viewport);
	RenderDevice.SetPipelineState(CommandList, PipelineState);
	const auto* ViewBytes = reinterpret_cast<const uint8*>(&ViewConstants);
	RenderDevice.SetConstantData(CommandList, EShaderStage::Vertex, ViewConstantsSlot, std::span<const uint8>(ViewBytes, sizeof(ViewConstants)));
	RenderDevice.SetConstantData(CommandList, EShaderStage::Pixel, ViewConstantsSlot, std::span<const uint8>(ViewBytes, sizeof(ViewConstants)));
	RenderDevice.Draw(CommandList, 6);
}
