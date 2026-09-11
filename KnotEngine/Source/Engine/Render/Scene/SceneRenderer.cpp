#include "Render/Scene/SceneRenderer.h"

#include "Core/Assert.h"
#include "Render/Renderer.h"
#include "Render/RHI/RenderDevice.h"
#include "Render/Scene/Scene.h"
#include "Render/Resource/MeshTypes.h"

#include <algorithm>
#include <bit>
#include <cmath>

FSceneRenderer::FSceneRenderer(const FSceneViewFamily& InViewFamily)
	: ViewFamily(InViewFamily)
{
	check(ViewFamily.Scene);
}

void FSceneRenderer::Render(URenderer& Renderer)
{
	IRenderDevice& RenderDevice = Renderer.GetRenderDevice();
	const FCommandListHandle CommandList = Renderer.GetCommandList();
	const FSceneRenderTarget& Target = ViewFamily.RenderTarget;

	check(CommandList.IsValid() && Target.Color.IsValid() && Target.Depth.IsValid() && Target.Width > 0 && Target.Height > 0);
	const FRenderViewport TargetViewport = { 0.0f, 0.0f, static_cast<float>(Target.Width), static_cast<float>(Target.Height), 0.0f, 1.0f };
	
	// Family 전체를 한 번 Clear한다. 여러 View가 같은 타깃의 서로 다른 영역을 사용할 수 있다.
	Renderer.BeginRenderTarget(Target.Color, Target.Depth, TargetViewport);
	for (const FSceneView& View : ViewFamily.Views)
	{
		check(View.Viewport.Width > 0.0f && View.Viewport.Height > 0.0f);
		check(View.Viewport.TopLeftX >= 0.0f && View.Viewport.TopLeftY >= 0.0f);
		check(View.Viewport.TopLeftX + View.Viewport.Width <= Target.Width);
		check(View.Viewport.TopLeftY + View.Viewport.Height <= Target.Height);

		RenderDevice.SetViewport(CommandList, View.Viewport);
		CullView(View);
		RenderOpaquePass(Renderer, View);
	}

	Renderer.EndRenderTarget();
}

// Frustum Culling을 수행하여 실제 그릴 프록시들을 수집한다.
void FSceneRenderer::CullView(const FSceneView& View)
{
	VisiblePrimitives.clear();
	if (!ViewFamily.ShowFlags.bPrimitive)
	{
		return;
	}
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

// 현재는 단일 불투명 패스를 실행하므로, 렌더패스를 따로 객체화하지 않고 정렬 후 단순 Render까지 실행한다.
void FSceneRenderer::RenderOpaquePass(URenderer& Renderer, const FSceneView& View)
{
	TArray<FMeshDrawCommand> OpaqueCommands;
	OpaqueCommands.reserve(VisiblePrimitives.size());

	// 양수 View depth의 비트 순서로 앞에서 뒤로 정렬한다.
	for (const FPrimitiveSceneProxy* Primitive : VisiblePrimitives)
	{
		const float Depth = View.ViewMatrix.TransformPosition(Primitive->WorldBounds.GetCenter()).Z;
		check(!std::isnan(Depth) && !std::isinf(Depth));
		const uint32 SortKey = std::bit_cast<uint32>(std::max(0.0f, Depth));
		OpaqueCommands.push_back({ Primitive, SortKey });
	}

	std::stable_sort(OpaqueCommands.begin(), OpaqueCommands.end(), [](const FMeshDrawCommand& Left, const FMeshDrawCommand& Right)
	{
		return Left.SortKey < Right.SortKey;
	});
	for (const FMeshDrawCommand& Command : OpaqueCommands)
	{
		Renderer.UpdateConstant(Command.Primitive->WorldMatrix * View.ViewProjectionMatrix);
		Renderer.DrawMeshBuffer(*Command.Primitive->Mesh->GetMeshBuffer());
	}
}
