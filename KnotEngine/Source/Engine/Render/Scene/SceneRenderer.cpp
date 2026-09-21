#include "Render/Scene/SceneRenderer.h"

#include "Core/Assert.h"
#include "Core/Profiling/CPUProfiler.h"
#include "Render/Graph/RenderGraph.h"
#include "Render/Pass/DebugDrawPass.h"
#include "Render/Pass/OverlayPass.h"
#include "Render/Pass/OpaquePass.h"
#include "Render/Pass/PostProcessPass.h"
#include "Render/Pass/SelectionPass.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Render/Renderer.h"
#include "Render/RHI/RenderDevice.h"
#include "Render/Scene/Scene.h"

FSceneRenderer::FSceneRenderer(const FSceneViewFamily& InViewFamily)
    : ViewFamily(InViewFamily)
{
	check(ViewFamily.Scene);
}

void FSceneRenderer::Render(FRenderer& Renderer)
{
	KNOT_PROFILE_SCOPE("Render", "FSceneRenderer::Render");

	const FCommandListHandle CommandList = Renderer.GetCommandList();
	const FSceneRenderTarget& Target = ViewFamily.RenderTarget;

	check(CommandList.IsValid() && Target.SceneColor.IsValid() && Target.DisplayColor.IsValid() && Target.SelectionDepth.IsValid() && Target.Depth.IsValid() && Target.Width > 0 && Target.Height > 0);
	const FRenderViewport TargetViewport = { 0.0f, 0.0f, static_cast<float>(Target.Width), static_cast<float>(Target.Height), 0.0f, 1.0f };

	// Family 전체를 한 번 Clear한다. 여러 View가 같은 타깃의 서로 다른 영역을 사용할 수 있다.
	Renderer.BeginRenderTarget(Target.SceneColor, Target.Depth, TargetViewport);
	Renderer.GetRenderDevice().ClearDepthStencil(CommandList, Target.SelectionDepth, 0.0f, 0);
	if (ViewFamily.ShowFlags.bBounds || !Renderer.GetDebugDraw().IsEmpty())
	{
		Renderer.GetDebugDraw().Prepare(Renderer.GetRenderDevice());
	}

	FRenderGraph RenderGraph;
	uint32 PreviousNode = FRenderGraph::InvalidIndex;
	TArray<uint32> OverlayNodes;
	for (const FSceneView& View : ViewFamily.Views)
	{
		bool bHasSelection = false;
		check(View.Viewport.Width > 0.0f && View.Viewport.Height > 0.0f);
		check(View.Viewport.TopLeftX >= 0.0f && View.Viewport.TopLeftY >= 0.0f);
		check(View.Viewport.TopLeftX + View.Viewport.Width <= Target.Width);
		check(View.Viewport.TopLeftY + View.Viewport.Height <= Target.Height);

		VisiblePrimitives.clear();
		if (ViewFamily.ShowFlags.bPrimitive || ViewFamily.ShowFlags.bBounds)
		{
			CullView(View);
		}
		if (ViewFamily.ShowFlags.bPrimitive)
		{
			const uint32 OpaqueNode = FOpaquePass::AddPass(RenderGraph, Renderer, View, VisiblePrimitives);
			if (PreviousNode != FRenderGraph::InvalidIndex)
			{
				RenderGraph.AddDependency(OpaqueNode, PreviousNode);
			}
			PreviousNode = OpaqueNode;

			const uint32 SelectionNode = FSelectionPass::AddPass(RenderGraph, Renderer, View, VisiblePrimitives, Target.SelectionDepth);
			bHasSelection = SelectionNode != FRenderGraph::InvalidIndex;
			if (SelectionNode != FRenderGraph::InvalidIndex)
			{
				RenderGraph.AddDependency(SelectionNode, PreviousNode);
				PreviousNode = SelectionNode;
			}
		}
		if (ViewFamily.ShowFlags.bGrid || ViewFamily.ShowFlags.bAxis)
		{
			OverlayNodes.push_back(FOverlayPass::AddPass(RenderGraph, Renderer, View, ViewFamily.ShowFlags, Target.DisplayColor, Target.Depth, Target.SelectionDepth, bHasSelection));
		}
		if (ViewFamily.ShowFlags.bBounds || !Renderer.GetDebugDraw().IsEmpty())
		{
			std::span<const FPrimitiveSceneProxy* const> BoundsPrimitives;
			if (ViewFamily.ShowFlags.bBounds)
			{
				BoundsPrimitives = VisiblePrimitives;
			}
			OverlayNodes.push_back(FDebugDrawPass::AddPass(RenderGraph, Renderer, View, BoundsPrimitives, Target.DisplayColor, Target.Depth));
		}
	}
	const uint32 PostProcessNode = FPostProcessPass::AddPass(RenderGraph, Renderer, Target.SceneColor, Target.DisplayColor, Target.SelectionDepth, Target.Depth, TargetViewport);
	if (PreviousNode != FRenderGraph::InvalidIndex)
	{
		RenderGraph.AddDependency(PostProcessNode, PreviousNode);
	}
	PreviousNode = PostProcessNode;
	for (const uint32 OverlayNode : OverlayNodes)
	{
		RenderGraph.AddDependency(OverlayNode, PreviousNode);
		PreviousNode = OverlayNode;
	}
	Renderer.Execute(RenderGraph);

	Renderer.EndRenderTarget();
}

// Frustum Culling을 수행하여 실제 그릴 프록시들을 수집한다.
void FSceneRenderer::CullView(const FSceneView& View)
{
	VisiblePrimitives.clear();
	for (const auto& Entry : ViewFamily.Scene->GetProxies())
	{
		const FPrimitiveSceneProxy& Primitive = *Entry;
		if (Primitive.bVisible && Primitive.WorldBounds.IsValid() && !View.Frustum.IsOutside(Primitive.WorldBounds))
		{
			VisiblePrimitives.push_back(&Primitive);
		}
	}
}
