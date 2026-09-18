#include "Render/Scene/SceneRenderer.h"

#include "Asset/Material/MaterialInterface.h"
#include "Asset/Texture/Texture.h"
#include "Core/Assert.h"
#include "Core/Profiling/CPUProfiler.h"
#include "Render/Graph/RenderGraph.h"
#include "Render/Pass/OverlayPass.h"
#include "Render/Pass/OpaquePass.h"
#include "Render/Pass/PostProcessPass.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Render/Renderer.h"
#include "Render/Resource/Material/Material.h"
#include "Render/Resource/Mesh/Mesh.h"
#include "Render/Scene/Scene.h"

FSceneRenderer::FSceneRenderer(const FSceneViewFamily& InViewFamily)
	: ViewFamily(InViewFamily)
{
	check(ViewFamily.Scene);
}

void FSceneRenderer::Render(URenderer& Renderer)
{
	KNOT_PROFILE_SCOPE("Render", "FSceneRenderer::Render");

	const FCommandListHandle CommandList = Renderer.GetCommandList();
	const FSceneRenderTarget& Target = ViewFamily.RenderTarget;

	check(CommandList.IsValid() && Target.SceneColor.IsValid() && Target.DisplayColor.IsValid() && Target.Depth.IsValid() && Target.Width > 0 && Target.Height > 0);
	const FRenderViewport TargetViewport = { 0.0f, 0.0f, static_cast<float>(Target.Width), static_cast<float>(Target.Height), 0.0f, 1.0f };

	// Family 전체를 한 번 Clear한다. 여러 View가 같은 타깃의 서로 다른 영역을 사용할 수 있다.
	Renderer.BeginRenderTarget(Target.SceneColor, Target.Depth, TargetViewport);

	FRenderGraph RenderGraph;
	uint32 PreviousNode = FRenderGraph::InvalidIndex;
	TArray<uint32> OverlayNodes;
	for (const FSceneView& View : ViewFamily.Views)
	{
		check(View.Viewport.Width > 0.0f && View.Viewport.Height > 0.0f);
		check(View.Viewport.TopLeftX >= 0.0f && View.Viewport.TopLeftY >= 0.0f);
		check(View.Viewport.TopLeftX + View.Viewport.Width <= Target.Width);
		check(View.Viewport.TopLeftY + View.Viewport.Height <= Target.Height);

		VisiblePrimitives.clear();
		if (ViewFamily.ShowFlags.bPrimitive || ViewFamily.ShowFlags.bBounds)
		{
			CullView(View);
		}
		Prepare(Renderer);
		if (ViewFamily.ShowFlags.bPrimitive)
		{
			const uint32 OpaqueNode = FOpaquePass::AddPass(RenderGraph, Renderer, View, VisiblePrimitives);
			if (PreviousNode != FRenderGraph::InvalidIndex)
			{
				RenderGraph.AddDependency(OpaqueNode, PreviousNode);
			}
			PreviousNode = OpaqueNode;
		}
		if (ViewFamily.ShowFlags.bGrid || ViewFamily.ShowFlags.bAxis || ViewFamily.ShowFlags.bBounds)
		{
			OverlayNodes.push_back(FOverlayPass::AddPass(RenderGraph, Renderer, View, ViewFamily.ShowFlags, VisiblePrimitives));
		}
	}
	const uint32 PostProcessNode = FPostProcessPass::AddPass(RenderGraph, Renderer, Target.SceneColor, Target.DisplayColor, Target.Depth, TargetViewport);
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

// 현재 View에서 사용할 Mesh와 Material Texture의 GPU 리소스를 Render Pass 구성 전에 준비한다.
void FSceneRenderer::Prepare(URenderer& Renderer)
{
	IRenderDevice& RenderDevice = Renderer.GetRenderDevice();
	if (ViewFamily.ShowFlags.bBounds)
	{
		panicf(Renderer.GetDebugBoundsMesh().InitResources(RenderDevice), "Bounds Mesh의 GPU Buffer 생성에 실패했다.");
	}
	if (!ViewFamily.ShowFlags.bPrimitive)
	{
		return;
	}

	for (const FPrimitiveSceneProxy* Primitive : VisiblePrimitives)
	{
		const auto& StaticMeshProxy = static_cast<const FStaticMeshSceneProxy&>(*Primitive);
		check(StaticMeshProxy.Mesh);
		panicf(StaticMeshProxy.Mesh->InitResources(RenderDevice), "Static Mesh의 GPU Buffer 생성에 실패했다.");

		const FStaticMeshLOD& LOD = StaticMeshProxy.Mesh->GetLOD(0);
		for (const FStaticMeshSection& Section : LOD.GetSections())
		{
			const UMaterialInterface* MaterialInterface = StaticMeshProxy.GetMaterial(Section.MaterialIndex);
			const FMaterial* Material = MaterialInterface ? MaterialInterface->GetMaterial() : nullptr;
			if (!Material || !Material->IsValid())
			{
				continue;
			}

			const FMaterialParameterLayout& Layout = Material->GetOrCreateParameterLayout(Renderer.GetShaderRegistry());
			for (const FMaterialTextureBinding& Binding : Layout.Textures)
			{
				const FTextureMaterialParameter* Parameter = MaterialInterface->FindTextureParameter(Binding.Name);
				if (Parameter && Parameter->Texture)
				{
					panicf(Parameter->Texture->InitResources(RenderDevice), "Material Texture의 GPU Resource 생성에 실패했다. Name={}", Binding.Name.ToString());
				}
			}
		}
	}
}

// Frustum Culling을 수행하여 실제 그릴 프록시들을 수집한다.
void FSceneRenderer::CullView(const FSceneView& View)
{
	VisiblePrimitives.clear();
	for (const auto& Entry : ViewFamily.Scene->GetProxies())
	{
		const FPrimitiveSceneProxy& Primitive = *Entry;
		if (Primitive.bVisible && Primitive.WorldBounds.IsValid()
			&& View.Frustum.Intersects(Primitive.WorldBounds) != FFrustum::EFrustumIntersectResult::Outside)
		{
			VisiblePrimitives.push_back(&Primitive);
		}
	}
}
