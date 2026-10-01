#include "Render/Pass/OverlayPass.h"

#include "Core/Assert.h"
#include "Render/Renderer.h"
#include "Render/RHI/RenderDevice.h"
#include "Render/Scene/SceneView.h"

#include <cmath>

uint32 FOverlayPass::AddPass(
	FRenderGraph& Graph,
	FRenderer& Renderer,
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
		static constexpr float BaseGridSpacing = 0.2f;
		static constexpr float TargetGridLinePixels = 16.0f;
		Parameters.OverlayConstants.GridSpacing = BaseGridSpacing;
		Parameters.OverlayConstants.MajorGridInterval = 5.0f;
		Parameters.OverlayConstants.GridPlane = static_cast<uint32>(View.GridPlane);
		Parameters.OverlayConstants.IsOrthographic = View.OrthoWidth > 0.0f ? 1u : 0u;
		if (View.OrthoWidth > 0.0f && View.Viewport.Width > 0.0f)
		{
			const float DesiredSpacing = View.OrthoWidth / View.Viewport.Width * TargetGridLinePixels;
			if (DesiredSpacing > BaseGridSpacing)
			{
				const float Magnitude = std::pow(10.0f, std::floor(std::log10(DesiredSpacing)));
				const float NormalizedSpacing = DesiredSpacing / Magnitude;
				const float Step = NormalizedSpacing <= 1.0f ? 1.0f : NormalizedSpacing <= 2.0f ? 2.0f : NormalizedSpacing <= 5.0f ? 5.0f : 10.0f;
				Parameters.OverlayConstants.GridSpacing = Step * Magnitude;
			}
		}
		Parameters.OverlayConstants.MinorColor = FVector4(0.30f, 0.33f, 0.38f, 0.3f);
		Parameters.OverlayConstants.MajorColor = FVector4(0.42f, 0.46f, 0.52f, 0.5f);
		Parameters.OverlayConstants.Projection = View.ProjectionMatrix;
		Parameters.OverlayConstants.InverseProjection = View.InverseProjectionMatrix;
		Parameters.OverlayConstants.InverseViewRotation = View.ViewMatrix.GetTransposed();
		Parameters.OverlayConstants.InverseViewRotation.M[0][3] = 0.0f;
		Parameters.OverlayConstants.InverseViewRotation.M[1][3] = 0.0f;
		Parameters.OverlayConstants.InverseViewRotation.M[2][3] = 0.0f;
		Parameters.OverlayConstants.InverseViewRotation.M[3][0] = 0.0f;
		Parameters.OverlayConstants.InverseViewRotation.M[3][1] = 0.0f;
		Parameters.OverlayConstants.InverseViewRotation.M[3][2] = 0.0f;
		Parameters.OverlayConstants.InverseViewRotation.M[3][3] = 1.0f;
		const float MajorGridSpacing = Parameters.OverlayConstants.GridSpacing * Parameters.OverlayConstants.MajorGridInterval;
		switch (View.GridPlane)
		{
		case EGridPlane::XZ:
			Parameters.OverlayConstants.GridOriginPhase = FVector2(
				std::fmod(View.ViewOrigin.X, MajorGridSpacing), std::fmod(View.ViewOrigin.Z, MajorGridSpacing));
			Parameters.OverlayConstants.CameraPlaneDistance = View.ViewOrigin.Y;
			break;
		case EGridPlane::YZ:
			Parameters.OverlayConstants.GridOriginPhase = FVector2(
				std::fmod(View.ViewOrigin.Y, MajorGridSpacing), std::fmod(View.ViewOrigin.Z, MajorGridSpacing));
			Parameters.OverlayConstants.CameraPlaneDistance = View.ViewOrigin.X;
			break;
		case EGridPlane::XY:
			Parameters.OverlayConstants.GridOriginPhase = FVector2(
				std::fmod(View.ViewOrigin.X, MajorGridSpacing), std::fmod(View.ViewOrigin.Y, MajorGridSpacing));
			Parameters.OverlayConstants.CameraPlaneDistance = View.ViewOrigin.Z;
			break;
		}
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
		View.InverseViewProjectionMatrix,
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
