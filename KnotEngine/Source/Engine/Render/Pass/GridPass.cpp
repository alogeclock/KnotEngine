#include "Render/Pass/GridPass.h"

#include "Core/Assert.h"
#include "Render/Renderer.h"
#include "Render/RHI/RenderDevice.h"
#include "Render/Scene/SceneView.h"
#include "Source/Resource/resource.h"

uint32 FGridPass::AddPass(FRenderGraph& Graph, URenderer& Renderer, const FSceneView& View)
{
	IRenderDevice* RenderDevice = &Renderer.GetRenderDevice();
	const FCommandListHandle CommandList = Renderer.GetCommandList();
	check(CommandList.IsValid());

	check(GShaderRegistry && GPipelineStateCache);
	const FShaderHandle VertexShader = GShaderRegistry->GetOrCreate({ IDR_GRID_SHADER, "Grid.hlsl", "VS", EShaderStage::Vertex });
	const FShaderHandle PixelShader = GShaderRegistry->GetOrCreate({ IDR_GRID_SHADER, "Grid.hlsl", "PS", EShaderStage::Pixel });
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
	const FPipelineStateHandle PipelineState = GPipelineStateCache->GetOrCreate(PipelineStateDesc);

	static constexpr float GridSpacing = 5.0f;
	static constexpr float MajorGridInterval = 10.0f;
	static constexpr float GridLineWidth = 1.0f;
	static constexpr float FadeDistance = 20000.0f;
	const FViewConstants ViewConstants = {
		View.ViewProjectionMatrix,
		View.ViewProjectionMatrix.GetInverse(),
		View.ViewOrigin,
		0.0f,
	};
	const FGridConstants GridConstants = {
		FadeDistance,
		GridSpacing,
		MajorGridInterval,
		GridLineWidth,
		FVector4(0.22f, 0.24f, 0.28f, 0.45f),
		FVector4(0.38f, 0.41f, 0.46f, 0.65f),
	};
	const FRenderViewport Viewport = View.Viewport;

	return Graph.AddPass("Grid", [RenderDevice, CommandList, PipelineState, Viewport, ViewConstants, GridConstants]()
	{
		ExecutePass(*RenderDevice, CommandList, PipelineState, Viewport, ViewConstants, GridConstants);
	});
}

void FGridPass::ExecutePass(
	IRenderDevice& RenderDevice,
	FCommandListHandle CommandList,
	FPipelineStateHandle PipelineState,
	const FRenderViewport& Viewport,
	const FViewConstants& ViewConstants,
	const FGridConstants& GridConstants)
{
	RenderDevice.SetViewport(CommandList, Viewport);
	RenderDevice.SetPipelineState(CommandList, PipelineState);
	const auto* ViewBytes = reinterpret_cast<const uint8*>(&ViewConstants);
	RenderDevice.SetConstantData(CommandList, EShaderStage::Pixel, ViewConstantsSlot, std::span<const uint8>(ViewBytes, sizeof(ViewConstants)));
	const auto* GridBytes = reinterpret_cast<const uint8*>(&GridConstants);
	RenderDevice.SetConstantData(CommandList, EShaderStage::Pixel, GridConstantsSlot, std::span<const uint8>(GridBytes, sizeof(GridConstants)));
	RenderDevice.Draw(CommandList, 3);
}
