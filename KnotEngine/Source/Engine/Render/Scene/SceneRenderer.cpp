#include "Render/Scene/SceneRenderer.h"

#include "Core/Assert.h"
#include "Render/Graph/RenderGraph.h"
#include "Render/Pass/AxisPass.h"
#include "Render/Pass/GridPass.h"
#include "Render/Pass/OpaquePass.h"
#include "Render/Renderer.h"
#include "Render/Scene/Scene.h"
#include "Render/Resource/MeshTypes.h"

FSceneRenderer::FSceneRenderer(const FSceneViewFamily& InViewFamily)
	: ViewFamily(InViewFamily)
{
	check(ViewFamily.Scene);
}

void FSceneRenderer::Render(URenderer& Renderer)
{
	const FCommandListHandle CommandList = Renderer.GetCommandList();
	const FSceneRenderTarget& Target = ViewFamily.RenderTarget;

	check(CommandList.IsValid() && Target.Color.IsValid() && Target.Depth.IsValid() && Target.Width > 0 && Target.Height > 0);
	const FRenderViewport TargetViewport = { 0.0f, 0.0f, static_cast<float>(Target.Width), static_cast<float>(Target.Height), 0.0f, 1.0f };
	
	// Family 전체를 한 번 Clear한다. 여러 View가 같은 타깃의 서로 다른 영역을 사용할 수 있다.
	Renderer.BeginRenderTarget(Target.Color, Target.Depth, TargetViewport);
	FRenderGraph RenderGraph;
	uint32 PreviousNode = FRenderGraph::InvalidIndex;
	for (const FSceneView& View : ViewFamily.Views)
	{
		check(View.Viewport.Width > 0.0f && View.Viewport.Height > 0.0f);
		check(View.Viewport.TopLeftX >= 0.0f && View.Viewport.TopLeftY >= 0.0f);
		check(View.Viewport.TopLeftX + View.Viewport.Width <= Target.Width);
		check(View.Viewport.TopLeftY + View.Viewport.Height <= Target.Height);

		VisiblePrimitives.clear();
		if (ViewFamily.ShowFlags.bPrimitive)
		{
			CullView(View);
			const uint32 OpaqueNode = FOpaquePass::AddPass(RenderGraph, Renderer, View, VisiblePrimitives);
			if (PreviousNode != FRenderGraph::InvalidIndex)
			{
				RenderGraph.AddDependency(OpaqueNode, PreviousNode);
			}
			PreviousNode = OpaqueNode;
		}
		if (ViewFamily.ShowFlags.bGrid)
		{
			const uint32 GridNode = FGridPass::AddPass(RenderGraph, Renderer, View);
			if (PreviousNode != FRenderGraph::InvalidIndex)
			{
				RenderGraph.AddDependency(GridNode, PreviousNode);
			}
			PreviousNode = GridNode;
		}
		if (ViewFamily.ShowFlags.bAxis)
		{
			const uint32 AxisNode = FAxisPass::AddPass(RenderGraph, Renderer, View);
			if (PreviousNode != FRenderGraph::InvalidIndex)
			{
				RenderGraph.AddDependency(AxisNode, PreviousNode);
			}
			PreviousNode = AxisNode;
		}
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
		if (Primitive.bVisible && Primitive.Mesh && Primitive.Mesh->GetMeshBuffer() && Primitive.WorldBounds.IsValid()
			&& View.Frustum.Intersects(Primitive.WorldBounds) != FFrustum::EFrustumIntersectResult::Outside)
		{
			VisiblePrimitives.push_back(&Primitive);
		}
	}
}
