#include "Render/Pass/PostProcessPass.h"

#include "Core/Assert.h"
#include "Render/Renderer.h"
#include "Render/RHI/RenderDevice.h"

// Scene Color와 Selection Depth를 읽어 Outline 및 Gamma Correction 결과를 Display Color에 기록하는 Graph Pass를 등록한다.
uint32 FPostProcessPass::AddPass(
	FRenderGraph& Graph,
	URenderer& Renderer,
	FTextureHandle SceneColor,
	FTextureHandle DisplayColor,
	FTextureHandle SelectionDepth,
	FTextureHandle DepthTarget,
	const FRenderViewport& Viewport)
{
	check(SceneColor.IsValid() && DisplayColor.IsValid() && SelectionDepth.IsValid() && DepthTarget.IsValid());
	check(SceneColor != DisplayColor);
	const FCommandListHandle CommandList = Renderer.GetCommandList();
	check(CommandList.IsValid());

	FShaderRegistry& ShaderRegistry = Renderer.GetShaderRegistry();
	FPipelineStateDesc PipelineStateDesc;
	PipelineStateDesc.VertexShader = ShaderRegistry.GetOrCreate({ "/Engine/Shader/PostProcess.hlsl", "VS", EShaderStage::Vertex });
	PipelineStateDesc.PixelShader = ShaderRegistry.GetOrCreate({ "/Engine/Shader/PostProcess.hlsl", "PS", EShaderStage::Pixel });
	PipelineStateDesc.DepthMode = EDepthMode::Disabled;
	PipelineStateDesc.RasterizerState.CullMode = ECullMode::None;
	const FPipelineStateHandle PipelineState = Renderer.GetPipelineStateCache().GetOrCreate(PipelineStateDesc);
	IRenderDevice* RenderDevice = &Renderer.GetRenderDevice();

	return Graph.AddPass("PostProcess", [RenderDevice, CommandList, PipelineState, SceneColor, DisplayColor, SelectionDepth, DepthTarget, Viewport]()
	{
		ExecutePass(*RenderDevice, CommandList, PipelineState, SceneColor, DisplayColor, SelectionDepth, DepthTarget, Viewport);
	});
}

// 전체 화면 삼각형을 그려 선택 효과를 합성하고 선형 Scene Color를 sRGB Display Color로 변환한다.
void FPostProcessPass::ExecutePass(
	IRenderDevice& RenderDevice,
	FCommandListHandle CommandList,
	FPipelineStateHandle PipelineState,
	FTextureHandle SceneColor,
	FTextureHandle DisplayColor,
	FTextureHandle SelectionDepth,
	FTextureHandle DepthTarget,
	const FRenderViewport& Viewport)
{
	RenderDevice.SetRenderTargets(CommandList, DisplayColor, DepthTarget);
	RenderDevice.SetViewport(CommandList, Viewport);
	RenderDevice.SetPipelineState(CommandList, PipelineState);
	RenderDevice.SetTexture(CommandList, EShaderStage::Pixel, 0, SceneColor);
	RenderDevice.SetTexture(CommandList, EShaderStage::Pixel, 1, SelectionDepth);
	RenderDevice.Draw(CommandList, 3);
	RenderDevice.SetTexture(CommandList, EShaderStage::Pixel, 0, {});
	RenderDevice.SetTexture(CommandList, EShaderStage::Pixel, 1, {});
}
