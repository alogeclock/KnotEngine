#include "Render/Scene/Scene.h"
#include "Core/Assert.h"
#include "Render/Renderer.h"
#include "Render/Resource/Mesh/StaticMeshResource.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <utility>
FScene::~FScene()
{
	// World는 Scene보다 먼저 Component 등록을 해제하여 Proxy를 제거한다.
	check(Proxies.empty() && PendingRenderCommands.empty() && PendingCommandIndices.empty() && PendingRayQueries.empty());
}

// 새 Primitive의 Add 명령을 등록하고 이후 갱신이 같은 명령에 병합되도록 인덱스를 저장한다.
uint64 FScene::AddPrimitive(std::unique_ptr<FPrimitiveSceneProxy> Proxy, FPrimitiveRenderData&& RenderData)
{
	check(Proxy);
	check(NextPrimitiveId != 0);
	const uint64 PrimitiveId = NextPrimitiveId++;
	check(!PendingCommandIndices.contains(PrimitiveId));
	PendingCommandIndices.emplace(PrimitiveId, PendingRenderCommands.size());
	PendingRenderCommands.push_back({ EPrimitiveCommandAction::Add, ERenderCommandType::All, PrimitiveId, std::move(RenderData), std::move(Proxy) });
	return PrimitiveId;
}

// Primitive별 Pending Command 인덱스로 기존 명령을 상수 시간에 찾고 최신 Render Data를 병합한다.
void FScene::UpdatePrimitive(uint64 PrimitiveId, ERenderCommandType Type, FPrimitiveRenderData&& RenderData)
{
	check(PrimitiveId != 0 && Type != ERenderCommandType::None);
	const auto Iterator = PendingCommandIndices.find(PrimitiveId);
	if (Iterator != PendingCommandIndices.end())
	{
		FPrimitiveRenderCommand& Command = PendingRenderCommands[Iterator->second];
		check(Command.PrimitiveId == PrimitiveId && Command.Action != EPrimitiveCommandAction::Remove);
		Merge(Command.RenderData, std::move(RenderData), Type);
		Command.Type |= Type;
		return;
	}
	PendingCommandIndices.emplace(PrimitiveId, PendingRenderCommands.size());
	PendingRenderCommands.push_back({ EPrimitiveCommandAction::Update, Type, PrimitiveId, std::move(RenderData) });
}

// Add 전 제거는 명령을 취소하고, 이미 존재하는 Primitive는 Pending 명령을 Remove 하나로 교체한다.
void FScene::RemovePrimitive(uint64 PrimitiveId)
{
	check(PrimitiveId != 0);
	const auto Iterator = PendingCommandIndices.find(PrimitiveId);
	if (Iterator == PendingCommandIndices.end())
	{
		PendingCommandIndices.emplace(PrimitiveId, PendingRenderCommands.size());
		PendingRenderCommands.push_back({ EPrimitiveCommandAction::Remove, ERenderCommandType::All, PrimitiveId });
		return;
	}

	const SIZE_T CommandIndex = Iterator->second;
	FPrimitiveRenderCommand& Command = PendingRenderCommands[CommandIndex];
	check(Command.PrimitiveId == PrimitiveId);
	if (Command.Action == EPrimitiveCommandAction::Add)
	{
		const SIZE_T LastIndex = PendingRenderCommands.size() - 1;
		PendingCommandIndices.erase(Iterator);
		if (CommandIndex != LastIndex)
		{
			PendingRenderCommands[CommandIndex] = std::move(PendingRenderCommands[LastIndex]);
			PendingCommandIndices[PendingRenderCommands[CommandIndex].PrimitiveId] = CommandIndex;
		}
		PendingRenderCommands.pop_back();
		return;
	}
	if (Command.Action == EPrimitiveCommandAction::Remove)
	{
		return;
	}
	Command = { EPrimitiveCommandAction::Remove, ERenderCommandType::All, PrimitiveId };
}

// 제출할 명령 배열을 분리하고 다음 프레임의 Primitive별 Pending Command 인덱스를 초기화한다.
TArray<FPrimitiveRenderCommand> FScene::DrainRenderCommands()
{
	TArray<FPrimitiveRenderCommand> Commands = std::move(PendingRenderCommands);
	PendingRenderCommands.clear();
	PendingCommandIndices.clear();
	return Commands;
}

// Render Thread의 프레임 시작 시 FIFO 명령을 적용한다.
void FScene::ApplyRenderCommands(FRenderer& Renderer, TArray<FPrimitiveRenderCommand>&& Commands)
{
	for (FPrimitiveRenderCommand& Command : Commands)
	{
		if (Command.Action == EPrimitiveCommandAction::Add)
		{
			check(Command.Proxy && !ProxyIndices.contains(Command.PrimitiveId));
			Command.Proxy->Apply(ERenderCommandType::All, Command.RenderData, Renderer);
			
			Command.Proxy->PrimitiveId = Command.PrimitiveId;
			if (Command.Proxy->bVisible && Command.Proxy->WorldBounds.IsValid())
			{
				BVH.Update(Command.PrimitiveId, Command.Proxy->WorldBounds);
			}

			Command.Proxy->SceneIndex = Proxies.size();
			ProxyIndices.emplace(Command.PrimitiveId, Command.Proxy->SceneIndex);
			Proxies.push_back(std::move(Command.Proxy));

			continue;
		}

		const auto Iterator = ProxyIndices.find(Command.PrimitiveId);
		check(Iterator != ProxyIndices.end());
		const SIZE_T Index = Iterator->second;

		if (Command.Action == EPrimitiveCommandAction::Update)
		{
			Proxies[Index]->Apply(Command.Type, Command.RenderData, Renderer);
			
			const FPrimitiveSceneProxy& Proxy = *Proxies[Index];
			if (Proxy.bVisible && Proxy.WorldBounds.IsValid())
			{
				BVH.Update(Command.PrimitiveId, Proxy.WorldBounds);
			}
			else
			{
				BVH.Remove(Command.PrimitiveId);
			}

			continue;
		}

		BVH.Remove(Command.PrimitiveId);

		const SIZE_T LastIndex = Proxies.size() - 1;
		if (Index != LastIndex)
		{
			std::swap(Proxies[Index], Proxies[LastIndex]);
			Proxies[Index]->SceneIndex = Index;
			ProxyIndices[Proxies[Index]->PrimitiveId] = Index;
		}
		Proxies.pop_back();
		ProxyIndices.erase(Command.PrimitiveId);
	}

	BVH.Prepare();
}

// GT에서 입력 시점의 Ray와 View를 대기 배열에 넣고, 나중에 완료 여부와 결과를 읽을 수 있도록 같은 Query를 반환한다.
std::shared_ptr<FSceneRayQuery> FScene::EnqueueRayQuery(const FRay& Ray, const FSceneView& View)
{
	auto Query = std::make_shared<FSceneRayQuery>();
	Query->Ray = Ray;
	Query->View = View;
	PendingRayQueries.push_back(Query);
	return Query;
}

// GT의 대기 요청 배열을 RT 제출용으로 꺼내 반환하고, 다음 요청을 받을 대기 배열을 비운다.
TArray<std::shared_ptr<FSceneRayQuery>> FScene::DrainRayQueries()
{
	TArray<std::shared_ptr<FSceneRayQuery>> Requests = std::move(PendingRayQueries);
	PendingRayQueries.clear();
	return Requests;
}

// RT에서 요청 View의 렌더링 기준으로 선택한 LOD 삼각형을 검사하고, 가장 가까운 Primitive ID와 교차점을 기록한 뒤 완료 표시를 설정한다.
void FScene::ExecuteRayQueries(const TArray<std::shared_ptr<FSceneRayQuery>>& Requests)
{
	TArray<FSceneBVHRayHit> Candidates;
	for (const std::shared_ptr<FSceneRayQuery>& Request : Requests)
	{
		FSceneRayQuery& Query = *Request;
		FSceneBVHQueryStatistics Statistics;
		BVH.RayQuery(Query.Ray, Candidates, Statistics);
		float ClosestDistance = (std::numeric_limits<float>::max)();

		for (const FSceneBVHRayHit& Candidate : Candidates)
		{
			if (Candidate.Distance > ClosestDistance)
			{
				break;
			}
			const FStaticMeshSceneProxy& Proxy = static_cast<const FStaticMeshSceneProxy&>(*Proxies[ProxyIndices.at(Candidate.PrimitiveId)]);
			if (!Proxy.MeshResource || !Proxy.MeshResource->IsValid() || std::fabs(Proxy.WorldMatrix.GetDeterminant()) <= KMath::Epsilon)
			{
				continue;
			}
			const FMatrix Inverse = Proxy.WorldMatrix.GetInverse();
			// 축마다 Scale이 달라도 교차 거리를 World Ray와 비교할 수 있도록, 변환한 방향의 길이를 그대로 유지한다.
			const FRay LocalRay(Inverse.TransformPosition(Query.Ray.Origin), Inverse.TransformVector(Query.Ray.Direction));
			const FStaticMeshLOD& LOD = Proxy.MeshResource->GetMeshData()->GetLOD(Proxy.SelectLOD(Query.View));
			const auto& Vertices = LOD.GetVertices();
			const auto& Indices = LOD.GetIndices();
			const SIZE_T Count = Indices.empty() ? Vertices.size() : Indices.size();
			for (SIZE_T Index = 0; Index + 2 < Count; Index += 3)
			{
				const SIZE_T A = Indices.empty() ? Index : Indices[Index];
				const SIZE_T B = Indices.empty() ? Index + 1 : Indices[Index + 1];
				const SIZE_T C = Indices.empty() ? Index + 2 : Indices[Index + 2];
				if (A >= Vertices.size() || B >= Vertices.size() || C >= Vertices.size())
				{
					continue;
				}
				float Distance = 0.0f;
				if (LocalRay.Intersect(Vertices[A].Position, Vertices[B].Position, Vertices[C].Position, Distance) && Distance < ClosestDistance)
				{
					ClosestDistance = Distance;
					Query.PrimitiveId = Candidate.PrimitiveId;
					Query.HitPosition = Query.Ray.Origin + Query.Ray.Direction * Distance;
				}
			}
		}

		Query.bReady.store(true, std::memory_order_release);
	}
}

// RT에서 Frustum과 겹치는 Proxy를 반환하며, bInsideOnly가 true면 Bounds 전체가 포함된 Proxy만 반환한다.
FSceneBVHQueryStatistics FScene::QueryFrustum(const FFrustum& Frustum, TArray<const FPrimitiveSceneProxy*>& OutPrimitives, bool bInsideOnly)
{
	const auto Start = std::chrono::steady_clock::now();
	TArray<uint64> Ids;
	FSceneBVHQueryStatistics Statistics;
	BVH.FrustumQuery(Frustum, Ids, Statistics, bInsideOnly);

	OutPrimitives.clear();
	OutPrimitives.reserve(Ids.size());
	for (uint64 Id : Ids)
	{
		OutPrimitives.push_back(Proxies[ProxyIndices.at(Id)].get());
	}

	Statistics.TimeMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Start).count();
	QueryStatistics.Frustum.QueryCount += Statistics.QueryCount;
	QueryStatistics.Frustum.BoundsTests += Statistics.BoundsTests;
	QueryStatistics.Frustum.Candidates += Statistics.Candidates;
	QueryStatistics.Frustum.TimeMs += Statistics.TimeMs;
	return Statistics;
}

// RT에서 Sphere와 World Bounds가 겹치는 Proxy를 반환하며 메시 표면은 검사하지 않는다.
FSceneBVHQueryStatistics FScene::QuerySphere(const FVector& Center, float Radius, TArray<const FPrimitiveSceneProxy*>& OutPrimitives)
{
	const auto Start = std::chrono::steady_clock::now();
	TArray<uint64> Ids;
	FSceneBVHQueryStatistics Statistics;
	BVH.SphereQuery(Center, Radius, Ids, Statistics);

	OutPrimitives.clear();
	OutPrimitives.reserve(Ids.size());
	for (uint64 Id : Ids)
	{
		OutPrimitives.push_back(Proxies[ProxyIndices.at(Id)].get());
	}

	Statistics.TimeMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Start).count();
	QueryStatistics.Sphere.QueryCount += Statistics.QueryCount;
	QueryStatistics.Sphere.BoundsTests += Statistics.BoundsTests;
	QueryStatistics.Sphere.Candidates += Statistics.Candidates;
	QueryStatistics.Sphere.TimeMs += Statistics.TimeMs;
	return Statistics;
}

// RT에서 회전된 박스와 World Bounds가 겹치는 Proxy를 반환하며 메시 표면은 검사하지 않는다.
FSceneBVHQueryStatistics FScene::QueryOBB(const FOBB& Box, TArray<const FPrimitiveSceneProxy*>& OutPrimitives)
{
	const auto Start = std::chrono::steady_clock::now();
	TArray<uint64> Ids;
	FSceneBVHQueryStatistics Statistics;
	BVH.OBBQuery(Box, Ids, Statistics);

	OutPrimitives.clear();
	OutPrimitives.reserve(Ids.size());
	for (uint64 Id : Ids)
	{
		OutPrimitives.push_back(Proxies[ProxyIndices.at(Id)].get());
	}

	Statistics.TimeMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Start).count();
	QueryStatistics.OBB.QueryCount += Statistics.QueryCount;
	QueryStatistics.OBB.BoundsTests += Statistics.BoundsTests;
	QueryStatistics.OBB.Candidates += Statistics.Candidates;
	QueryStatistics.OBB.TimeMs += Statistics.TimeMs;
	return Statistics;
}

// RT에서 World Bounds를 Extent만큼 확장해 선분과 겹치는 Proxy를 반환하며, Extent가 0이면 일반 선분 검사다.
FSceneBVHQueryStatistics FScene::QuerySegment(
	const FVector& Start,
	const FVector& End,
	const FVector& Extent,
	TArray<const FPrimitiveSceneProxy*>& OutPrimitives)
{
	const auto QueryStart = std::chrono::steady_clock::now();
	TArray<uint64> Ids;
	FSceneBVHQueryStatistics Statistics;
	BVH.SegmentQuery(Start, End, Extent, Ids, Statistics);

	OutPrimitives.clear();
	OutPrimitives.reserve(Ids.size());
	for (uint64 Id : Ids)
	{
		OutPrimitives.push_back(Proxies[ProxyIndices.at(Id)].get());
	}

	Statistics.TimeMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - QueryStart).count();
	QueryStatistics.Segment.QueryCount += Statistics.QueryCount;
	QueryStatistics.Segment.BoundsTests += Statistics.BoundsTests;
	QueryStatistics.Segment.Candidates += Statistics.Candidates;
	QueryStatistics.Segment.TimeMs += Statistics.TimeMs;
	return Statistics;
}

void FScene::Merge(FPrimitiveRenderData& Destination, FPrimitiveRenderData&& Source, ERenderCommandType Type)
{
	if (HasRenderCommand(Type, ERenderCommandType::Transform))
	{
		Destination.WorldMatrix = Source.WorldMatrix;
	}

	if (HasRenderCommand(Type, ERenderCommandType::Mesh))
	{
		Destination.WorldMatrix = Source.WorldMatrix;
		Destination.LocalBounds = Source.LocalBounds;
		Destination.MeshAssetId = Source.MeshAssetId;
		Destination.bLODEnable = Source.bLODEnable;
	}

	if (HasRenderCommand(Type, ERenderCommandType::Material))
	{
		Destination.MaterialAssetIds = std::move(Source.MaterialAssetIds);
	}

	if (HasRenderCommand(Type, ERenderCommandType::Visibility))
	{
		Destination.bVisible = Source.bVisible;
		Destination.bSelected = Source.bSelected;
	}
}
