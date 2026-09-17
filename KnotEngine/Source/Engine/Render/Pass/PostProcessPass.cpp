#include "Render/Pass/PostProcessPass.h"

#include "Core/Assert.h"
#include "Render/Renderer.h"
#include "Render/RHI/RenderDevice.h"

// Scene Color를 읽어 Gamma Correction 결과를 Display Color에 기록하는 Graph Pass를 등록한다.
uint32 FPostProcessPass::AddPass(
	FRenderGraph& Graph,
	URenderer& Renderer,
	FTextureHandle SceneColor,
	FTextureHandle DisplayColor,
	FTextureHandle DepthTarget,
	const FRenderViewport& Viewport)
{
	check(SceneColor.IsValid() && DisplayColor.IsValid() && DepthTarget.IsValid());
	check(SceneColor != DisplayColor);
	const FCommandListHandle CommandList = Renderer.GetCommandList();
	check(CommandList.IsValid());

	FShaderRegistry& ShaderRegistry = Renderer.GetShaderRegistry();
	FPipelineStateDesc PipelineStateDesc;
	PipelineStateDesc.VertexShader = ShaderRegistry.GetOrCreate({ "/Engine/Shader/PostProcess.hlsl", "VS", EShaderStage::Vertex });
	PipelineStateDesc.PixelShader = ShaderRegistry.GetOrCreate({ "/Engine/Shader/PostProcess.hlsl", "PSGammaCorrection", EShaderStage::Pixel });
	PipelineStateDesc.bDepthTestEnabled = false;
	PipelineStateDesc.bDepthWriteEnabled = false;
	PipelineStateDesc.RasterizerState.CullMode = ECullMode::None;
	const FPipelineStateHandle PipelineState = Renderer.GetPipelineStateCache().GetOrCreate(PipelineStateDesc);
	IRenderDevice* RenderDevice = &Renderer.GetRenderDevice();

	return Graph.AddPass("PostProcess", [RenderDevice, CommandList, PipelineState, SceneColor, DisplayColor, DepthTarget, Viewport]()
	{
		ExecutePass(*RenderDevice, CommandList, PipelineState, SceneColor, DisplayColor, DepthTarget, Viewport);
	});
}

// 전체 화면 삼각형을 그려 선형 Scene Color를 sRGB Display Color로 변환한다.
void FPostProcessPass::ExecutePass(
	IRenderDevice& RenderDevice,
	FCommandListHandle CommandList,
	FPipelineStateHandle PipelineState,
	FTextureHandle SceneColor,
	FTextureHandle DisplayColor,
	FTextureHandle DepthTarget,
	const FRenderViewport& Viewport)
{
	RenderDevice.SetRenderTargets(CommandList, DisplayColor, DepthTarget);
	RenderDevice.SetViewport(CommandList, Viewport);
	RenderDevice.SetPipelineState(CommandList, PipelineState);
	RenderDevice.SetTexture(CommandList, EShaderStage::Pixel, 0, SceneColor);
	RenderDevice.Draw(CommandList, 3);
	RenderDevice.SetTexture(CommandList, EShaderStage::Pixel, 0, {});
}
