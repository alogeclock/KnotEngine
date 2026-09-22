#include "Render/Pass/GizmoPass.h"

#include "Core/Assert.h"
#include "Render/Renderer.h"
#include "Render/RHI/RenderDevice.h"
#include "Render/Scene/SceneView.h"

// Transform Gizmo의 Procedural Geometry Pass를 Render Graph에 추가한다.
uint32 FGizmoPass::AddPass(FRenderGraph& Graph, FRenderer& Renderer, const FSceneView& View, FTextureHandle ColorTarget, FTextureHandle DepthTarget)
{
	check(View.Gizmo.bVisible && ColorTarget.IsValid() && DepthTarget.IsValid());
	IRenderDevice* RenderDevice = &Renderer.GetRenderDevice();
	const FCommandListHandle CommandList = Renderer.GetCommandList();
	FShaderRegistry& ShaderRegistry = Renderer.GetShaderRegistry();
	FPipelineStateCache& PipelineStateCache = Renderer.GetPipelineStateCache();

	FPipelineStateDesc PipelineDesc;
	PipelineDesc.VertexShader = ShaderRegistry.GetOrCreate({ "/Engine/Shader/Gizmo.hlsl", "GizmoVS", EShaderStage::Vertex });
	PipelineDesc.PixelShader = ShaderRegistry.GetOrCreate({ "/Engine/Shader/Gizmo.hlsl", "GizmoPS", EShaderStage::Pixel });
	PipelineDesc.PrimitiveTopology = EPrimitiveTopology::TriangleList;
	PipelineDesc.DepthMode = EDepthMode::Disabled;
	PipelineDesc.RasterizerState.CullMode = ECullMode::None;
	PipelineDesc.BlendState.RenderTarget.bBlendEnabled = true;
	PipelineDesc.BlendState.RenderTarget.SourceColorBlend = EBlendFactor::SourceAlpha;
	PipelineDesc.BlendState.RenderTarget.DestinationColorBlend = EBlendFactor::InverseSourceAlpha;
	PipelineDesc.BlendState.RenderTarget.SourceAlphaBlend = EBlendFactor::One;
	PipelineDesc.BlendState.RenderTarget.DestinationAlphaBlend = EBlendFactor::InverseSourceAlpha;
	const FPipelineStateHandle Pipeline = PipelineStateCache.GetOrCreate(PipelineDesc);

	FGizmoConstants Constants;
	Constants.Origin = View.Gizmo.Origin;
	Constants.AxisX = View.Gizmo.AxisX;
	Constants.AxisY = View.Gizmo.AxisY;
	Constants.AxisZ = View.Gizmo.AxisZ;
	Constants.WorldScale = View.Gizmo.WorldScale;
	const FMatrix InverseView = View.ViewMatrix.GetInverse();
	Constants.ViewRight = InverseView.GetScaledAxis(EAxis::X).GetSafeNormal();
	Constants.Mode = static_cast<uint32>(View.Gizmo.Mode);
	Constants.ViewUp = InverseView.GetScaledAxis(EAxis::Y).GetSafeNormal();
	Constants.HighlightedAxis = View.Gizmo.HighlightedAxis;
	Constants.ViewOrigin = View.ViewOrigin;
	Constants.ViewportWidth = View.Viewport.Width;
	Constants.ViewportHeight = View.Viewport.Height;
	const uint32 VertexCount = View.Gizmo.Mode == EGizmoViewMode::Translate ? 480u : View.Gizmo.Mode == EGizmoViewMode::Rotate ? 1752u : 534u;
	const FRenderViewport Viewport = View.Viewport;
	const FMatrix ViewProjection = View.ViewProjectionMatrix;
	return Graph.AddPass("Gizmo", [RenderDevice, CommandList, ColorTarget, DepthTarget, Viewport, ViewProjection, Pipeline, Constants, VertexCount]()
	{
		ExecutePass(*RenderDevice, CommandList, ColorTarget, DepthTarget, Viewport, ViewProjection, Pipeline, Constants, VertexCount);
	});
}

// Gizmo를 항상 Scene 위에 표시하고 Shader가 생성한 Handle Geometry를 그린다.
void FGizmoPass::ExecutePass(
	IRenderDevice& RenderDevice,
	FCommandListHandle CommandList,
	FTextureHandle ColorTarget,
	FTextureHandle DepthTarget,
	const FRenderViewport& Viewport,
	const FMatrix& ViewProjection,
	FPipelineStateHandle Pipeline,
	const FGizmoConstants& Constants,
	uint32 VertexCount)
{
	RenderDevice.SetRenderTargets(CommandList, ColorTarget, DepthTarget);
	RenderDevice.SetViewport(CommandList, Viewport);
	RenderDevice.SetPipelineState(CommandList, Pipeline);
	const auto* ViewBytes = reinterpret_cast<const uint8*>(&ViewProjection);
	const auto* GizmoBytes = reinterpret_cast<const uint8*>(&Constants);
	RenderDevice.SetConstantData(CommandList, EShaderStage::Vertex, 0, std::span<const uint8>(ViewBytes, sizeof(ViewProjection)));
	RenderDevice.SetConstantData(CommandList, EShaderStage::Vertex, 1, std::span<const uint8>(GizmoBytes, sizeof(Constants)));
	RenderDevice.Draw(CommandList, VertexCount);
}
