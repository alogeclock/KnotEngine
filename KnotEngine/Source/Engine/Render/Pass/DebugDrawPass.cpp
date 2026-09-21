#include "Render/Pass/DebugDrawPass.h"

#include "Core/Assert.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Render/Renderer.h"
#include "Render/Resource/Mesh/MeshBuffer.h"
#include "Render/Resource/Mesh/Vertex.h"
#include "Render/RHI/RenderDevice.h"
#include "Render/Scene/SceneView.h"

#include <algorithm>

uint32 FDebugDrawPass::AddPass(
	FRenderGraph& Graph,
	FRenderer& Renderer,
	const FSceneView& View,
	std::span<const FPrimitiveSceneProxy* const> BoundsPrimitives,
	FTextureHandle ColorTarget,
	FTextureHandle DepthTarget)
{
	check(ColorTarget.IsValid() && DepthTarget.IsValid());
	IRenderDevice* RenderDevice = &Renderer.GetRenderDevice();
	const FCommandListHandle CommandList = Renderer.GetCommandList();
	check(CommandList.IsValid());

	FDebugDraw& DebugDraw = Renderer.GetDebugDraw();
	FShaderRegistry& ShaderRegistry = Renderer.GetShaderRegistry();
	FPipelineStateCache& PipelineStateCache = Renderer.GetPipelineStateCache();
	const FShaderHandle LineVertexShader = ShaderRegistry.GetOrCreate({ "/Engine/Shader/DebugDraw.hlsl", "LineVS", EShaderStage::Vertex });
	const FShaderHandle InstanceVertexShader = ShaderRegistry.GetOrCreate({ "/Engine/Shader/DebugDraw.hlsl", "InstanceVS", EShaderStage::Vertex });
	const FShaderHandle PixelShader = ShaderRegistry.GetOrCreate({ "/Engine/Shader/DebugDraw.hlsl", "PS", EShaderStage::Pixel });

	FPassParameters Parameters;
	FPipelineStateDesc PipelineDesc;
	PipelineDesc.PixelShader = PixelShader;
	PipelineDesc.VertexLayout = FGeometryVertex::GetVertexLayout();
	PipelineDesc.PrimitiveTopology = EPrimitiveTopology::LineList;
	PipelineDesc.DepthMode = EDepthMode::ReadOnly;
	PipelineDesc.BlendState.RenderTarget.bBlendEnabled = true;
	PipelineDesc.BlendState.RenderTarget.SourceColorBlend = EBlendFactor::SourceAlpha;
	PipelineDesc.BlendState.RenderTarget.DestinationColorBlend = EBlendFactor::InverseSourceAlpha;
	PipelineDesc.BlendState.RenderTarget.SourceAlphaBlend = EBlendFactor::One;
	PipelineDesc.BlendState.RenderTarget.DestinationAlphaBlend = EBlendFactor::InverseSourceAlpha;
	PipelineDesc.RasterizerState.CullMode = ECullMode::None;
	PipelineDesc.RasterizerState.bAntialiasedLineEnabled = false;

	PipelineDesc.VertexShader = LineVertexShader;
	Parameters.LinePipeline = PipelineStateCache.GetOrCreate(PipelineDesc);
	PipelineDesc.VertexShader = InstanceVertexShader;
	Parameters.InstancePipeline = PipelineStateCache.GetOrCreate(PipelineDesc);
	Parameters.LineBuffer = DebugDraw.LineBuffer;
	Parameters.LineVertexCount = static_cast<uint32>(DebugDraw.LineVertices.size());

	for (uint32 ShapeIndex = 0; ShapeIndex < FDebugDraw::ShapeTypeCount; ++ShapeIndex)
	{
		const auto ShapeType = static_cast<FDebugDraw::EShapeType>(ShapeIndex);
		const TArray<FDebugDraw::FInstance>& SourceInstances = DebugDraw.Instances[ShapeIndex];
		if (!SourceInstances.empty())
		{
			const FMeshBuffer& MeshBuffer = DebugDraw.GetShapeMesh(ShapeType).GetMeshBuffer();
			Parameters.ShapeBatches.push_back({ &MeshBuffer, SourceInstances });
		}
	}

	if (!BoundsPrimitives.empty())
	{
		static const FVector4 BoundsColor = FColor(255, 196, 64).ToVector4();
		FShapeBatch BoundsBatch;
		BoundsBatch.MeshBuffer = &DebugDraw.CubeMesh.GetMeshBuffer();
		BoundsBatch.Instances.reserve(BoundsPrimitives.size());
		for (const FPrimitiveSceneProxy* Primitive : BoundsPrimitives)
		{
			const FVector Center = Primitive->WorldBounds.GetCenter();
			const FVector Extent = Primitive->WorldBounds.GetExtent();
			BoundsBatch.Instances.push_back({ FMatrix::MakeScale(Extent) * FMatrix::MakeTranslation(Center), BoundsColor, FVector4() });
		}
		Parameters.ShapeBatches.push_back(std::move(BoundsBatch));
	}

	const FViewConstants ViewConstants = {
		View.ViewProjectionMatrix,
		View.ViewProjectionMatrix.GetInverse(),
		View.ViewOrigin,
		View.FarClip,
	};
	const FRenderViewport Viewport = View.Viewport;
	return Graph.AddPass("DebugDraw", [RenderDevice, CommandList, ColorTarget, DepthTarget, Viewport, ViewConstants, Parameters = std::move(Parameters)]()
	{
		ExecutePass(*RenderDevice, CommandList, ColorTarget, DepthTarget, Viewport, ViewConstants, Parameters);
	});
}

void FDebugDrawPass::ExecutePass(
	IRenderDevice& RenderDevice,
	FCommandListHandle CommandList,
	FTextureHandle ColorTarget,
	FTextureHandle DepthTarget,
	const FRenderViewport& Viewport,
	const FViewConstants& ViewConstants,
	const FPassParameters& Parameters)
{
	RenderDevice.SetRenderTargets(CommandList, ColorTarget, DepthTarget);
	RenderDevice.SetViewport(CommandList, Viewport);
	DrawLines(RenderDevice, CommandList, ViewConstants, Parameters);
	DrawShapes(RenderDevice, CommandList, ViewConstants, Parameters);
}

void FDebugDrawPass::DrawLines(
	IRenderDevice& RenderDevice,
	FCommandListHandle CommandList,
	const FViewConstants& ViewConstants,
	const FPassParameters& Parameters)
{
	if (Parameters.LineVertexCount == 0)
	{
		return;
	}
	check(Parameters.LineBuffer.IsValid());
	RenderDevice.SetPipelineState(CommandList, Parameters.LinePipeline);
	const auto* ViewBytes = reinterpret_cast<const uint8*>(&ViewConstants);
	RenderDevice.SetConstantData(CommandList, EShaderStage::Vertex, ViewConstantsSlot, std::span<const uint8>(ViewBytes, sizeof(ViewConstants)));
	RenderDevice.SetVertexBuffer(CommandList, Parameters.LineBuffer, sizeof(FGeometryVertex));
	RenderDevice.Draw(CommandList, Parameters.LineVertexCount);
}

void FDebugDrawPass::DrawShapes(
	IRenderDevice& RenderDevice,
	FCommandListHandle CommandList,
	const FViewConstants& ViewConstants,
	const FPassParameters& Parameters)
{
	if (Parameters.ShapeBatches.empty())
	{
		return;
	}
	RenderDevice.SetPipelineState(CommandList, Parameters.InstancePipeline);
	const auto* ViewBytes = reinterpret_cast<const uint8*>(&ViewConstants);
	RenderDevice.SetConstantData(CommandList, EShaderStage::Vertex, ViewConstantsSlot, std::span<const uint8>(ViewBytes, sizeof(ViewConstants)));

	for (const FShapeBatch& Batch : Parameters.ShapeBatches)
	{
		check(Batch.MeshBuffer && Batch.MeshBuffer->IsValid());
		RenderDevice.SetVertexBuffer(CommandList, Batch.MeshBuffer->GetVertexBuffer().GetHandle(), Batch.MeshBuffer->GetStride());
		RenderDevice.SetIndexBuffer(CommandList, Batch.MeshBuffer->GetIndexBuffer().GetHandle(), EIndexFormat::UInt32);
		for (SIZE_T FirstInstance = 0; FirstInstance < Batch.Instances.size(); FirstInstance += MaxInstancesPerDraw)
		{
			const uint32 InstanceCount = static_cast<uint32>(std::min<SIZE_T>(MaxInstancesPerDraw, Batch.Instances.size() - FirstInstance));
			const auto* InstanceBytes = reinterpret_cast<const uint8*>(Batch.Instances.data() + FirstInstance);
			RenderDevice.SetConstantData(
				CommandList,
				EShaderStage::Vertex,
				InstanceConstantsSlot,
				std::span<const uint8>(InstanceBytes, InstanceCount * sizeof(FDebugDraw::FInstance)));
			RenderDevice.DrawIndexedInstanced(CommandList, Batch.MeshBuffer->GetIndexCount(), InstanceCount);
		}
	}
}
