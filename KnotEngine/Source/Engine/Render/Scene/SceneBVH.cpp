#include "Render/Scene/SceneBVH.h"

#include "Core/Assert.h"

#include <algorithm>
#include <chrono>

// 처음 받은 ID는 등록하고, 기존 ID의 Bounds가 바뀌면 해당 Leaf와 부모 노드들의 Bounds를 다시 계산한다.
void FSceneBVH::Update(uint64 PrimitiveId, const FAABB& Bounds)
{
	check(PrimitiveId != 0 && Bounds.IsValid());

	const auto Iterator = EntryIndices.find(PrimitiveId);
	if (Iterator == EntryIndices.end())
	{
		EntryIndices.emplace(PrimitiveId, Entries.size());
		Entries.push_back({ PrimitiveId, Bounds });
		bTopologyDirty = true;

		return;
	}

	FEntry& Entry = Entries[Iterator->second];
	if (Entry.Bounds.Equals(Bounds, 0.0f))
	{
		return;
	}

	Entry.Bounds = Bounds;
	if (!bTopologyDirty)
	{
		Nodes[Entry.Leaf].Bounds = Bounds;
		Refit(Nodes[Entry.Leaf].Parent);
	}
}

// 해당 ID를 제거하고 마지막 Entry로 빈자리를 채우며, 다음 Prepare에서 트리를 다시 만들도록 표시한다.
void FSceneBVH::Remove(uint64 PrimitiveId)
{
	const auto Iterator = EntryIndices.find(PrimitiveId);
	if (Iterator == EntryIndices.end())
	{
		return;
	}

	const SIZE_T Index = Iterator->second;
	if (Index + 1 != Entries.size())
	{
		Entries[Index] = Entries.back();
		EntryIndices[Entries[Index].PrimitiveId] = Index;
	}

	Entries.pop_back();
	EntryIndices.erase(PrimitiveId);
	bTopologyDirty = true;
}

// Primitive가 추가·제거되었으면 현재 Entry들로 트리를 다시 만들고, Bounds만 바뀌었으면 기존 트리를 유지한다.
void FSceneBVH::Prepare()
{
	if (!bTopologyDirty)
	{
		return;
	}

	Nodes.clear();
	Nodes.reserve(Entries.empty() ? 0 : Entries.size() * 2 - 1);

	TArray<int32> Order;
	Order.reserve(Entries.size());

	for (SIZE_T Index = 0; Index < Entries.size(); ++Index)
	{
		Order.push_back(static_cast<int32>(Index));
	}

	Root = Entries.empty() ? -1 : Build(Order, 0, Order.size(), -1);
	bTopologyDirty = false;
}

// RT에서 부모가 먼저 저장된 노드 배열을 순회해 전체 Bounds와 깊이를 수집하며 Query 통계에는 포함하지 않는다.
void FSceneBVH::CollectDebugBounds(TArray<FSceneBVHDebugBounds>& OutBounds)
{
	Prepare();
	OutBounds.clear();
	OutBounds.reserve(Nodes.size());
	for (const FNode& Node : Nodes)
	{
		const uint32 Depth = Node.Parent == -1 ? 0 : OutBounds[Node.Parent].Depth + 1;
		OutBounds.push_back({ Node.Bounds, Depth });
	}
}

// 중심 좌표가 가장 넓게 분포한 축을 골라 Primitive 개수를 반으로 나누며, Primitive 하나당 Leaf 하나를 만든다.
int32 FSceneBVH::Build(TArray<int32>& Order, SIZE_T Begin, SIZE_T End, int32 Parent)
{
	const int32 Index = static_cast<int32>(Nodes.size());
	Nodes.emplace_back();
	Nodes[Index].Parent = Parent;

	if (End - Begin == 1)
	{
		FEntry& Entry = Entries[Order[Begin]];
		Entry.Leaf = Index;
		Nodes[Index].Entry = Order[Begin];
		Nodes[Index].Bounds = Entry.Bounds;

		return Index;
	}

	FAABB Centers;
	for (SIZE_T Offset = Begin; Offset < End; ++Offset)
	{
		Centers.Expand(Entries[Order[Offset]].Bounds.GetCenter());
	}

	const FVector Size = Centers.Max - Centers.Min;
	const int32 Axis = Size.X > Size.Y ? (Size.X > Size.Z ? 0 : 2) : (Size.Y > Size.Z ? 1 : 2);
	const SIZE_T Middle = Begin + (End - Begin) / 2;

	std::nth_element(Order.begin() + Begin, Order.begin() + Middle, Order.begin() + End, [this, Axis](int32 Left, int32 Right)
	{
		const float A = Entries[Left].Bounds.GetCenter()[Axis];
		const float B = Entries[Right].Bounds.GetCenter()[Axis];

		return A == B ? Entries[Left].PrimitiveId < Entries[Right].PrimitiveId : A < B;
	});

	const int32 Left = Build(Order, Begin, Middle, Index);
	const int32 Right = Build(Order, Middle, End, Index);

	Nodes[Index].Left = Left;
	Nodes[Index].Right = Right;
	Nodes[Index].Bounds = Nodes[Left].Bounds;
	Nodes[Index].Bounds.Merge(Nodes[Right].Bounds);

	return Index;
}

// 지정한 노드부터 Root까지 올라가며 왼쪽·오른쪽 자식을 모두 감싸도록 부모 Bounds를 다시 계산한다.
void FSceneBVH::Refit(int32 NodeIndex)
{
	while (NodeIndex != -1)
	{
		FNode& Node = Nodes[NodeIndex];
		Node.Bounds = Nodes[Node.Left].Bounds;
		Node.Bounds.Merge(Nodes[Node.Right].Bounds);

		NodeIndex = Node.Parent;
	}
}

// Ray와 겹치는 Primitive ID와 Bounds 진입 거리를 가까운 순서로 반환하며, 삼각형은 검사하지 않는다.
void FSceneBVH::RayQuery(const FRay& Ray, TArray<FSceneBVHRayHit>& OutHits, FSceneBVHQueryStatistics& Statistics)
{
	Prepare();
	const auto Start = std::chrono::steady_clock::now();

	OutHits.clear();
	Statistics = {};
	Statistics.QueryCount = 1;
	TraversalStack.clear();

	if (Root != -1)
	{
		TraversalStack.push_back(Root);
	}

	while (!TraversalStack.empty())
	{
		const FNode& Node = Nodes[TraversalStack.back()];
		TraversalStack.pop_back();

		float Near = 0.0f;
		float Far = 0.0f;
		++Statistics.BoundsTests;

		if (!Node.Bounds.IntersectRay(Ray, Near, Far))
		{
			continue;
		}

		if (Node.Entry != -1)
		{
			OutHits.push_back({ Entries[Node.Entry].PrimitiveId, Near });
		}
		else
		{
			TraversalStack.push_back(Node.Left);
			TraversalStack.push_back(Node.Right);
		}
	}

	std::sort(OutHits.begin(), OutHits.end(), [](const FSceneBVHRayHit& A, const FSceneBVHRayHit& B)
	{
		return A.Distance == B.Distance ? A.PrimitiveId < B.PrimitiveId : A.Distance < B.Distance;
	});

	Statistics.Candidates = OutHits.size();
	Statistics.TimeMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Start).count();
}

// Frustum과 겹치는 Primitive ID를 반환하며, bInsideOnly가 true면 Bounds 전체가 Frustum 안에 있는 ID만 반환한다.
void FSceneBVH::FrustumQuery(const FFrustum& Frustum, TArray<uint64>& OutIds, FSceneBVHQueryStatistics& Statistics, bool bInsideOnly)
{
	Prepare();
	const auto Start = std::chrono::steady_clock::now();

	OutIds.clear();
	Statistics = {};
	Statistics.QueryCount = 1;
	TraversalStack.clear();

	if (Root != -1)
	{
		TraversalStack.push_back(Root);
	}

	while (!TraversalStack.empty())
	{
		const FNode& Node = Nodes[TraversalStack.back()];
		TraversalStack.pop_back();

		++Statistics.BoundsTests;
		const auto Intersection = Frustum.Intersects(Node.Bounds);
		if (Intersection == FFrustum::EFrustumIntersectResult::Outside)
		{
			continue;
		}

		if (Node.Entry != -1)
		{
			if (!bInsideOnly || Intersection == FFrustum::EFrustumIntersectResult::Inside)
			{
				OutIds.push_back(Entries[Node.Entry].PrimitiveId);
			}
		}
		else
		{
			TraversalStack.push_back(Node.Left);
			TraversalStack.push_back(Node.Right);
		}
	}

	Statistics.Candidates = OutIds.size();
	Statistics.TimeMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Start).count();
}

// 구 중심에서 Bounds의 가장 가까운 점까지 거리가 반지름 이하인 Primitive ID를 반환한다.
void FSceneBVH::SphereQuery(const FVector& Center, float Radius, TArray<uint64>& OutIds, FSceneBVHQueryStatistics& Statistics)
{
	Prepare();
	const auto Start = std::chrono::steady_clock::now();

	OutIds.clear();
	Statistics = {};
	Statistics.QueryCount = 1;
	TraversalStack.clear();

	if (Root != -1 && Radius >= 0.0f)
	{
		TraversalStack.push_back(Root);
	}

	while (!TraversalStack.empty())
	{
		const FNode& Node = Nodes[TraversalStack.back()];
		TraversalStack.pop_back();

		float DistanceSquared = 0.0f;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			const float Distance = Center[Axis] - std::clamp(Center[Axis], Node.Bounds.Min[Axis], Node.Bounds.Max[Axis]);
			DistanceSquared += Distance * Distance;
		}

		++Statistics.BoundsTests;

		if (DistanceSquared > Radius * Radius)
		{
			continue;
		}

		if (Node.Entry != -1)
		{
			OutIds.push_back(Entries[Node.Entry].PrimitiveId);
		}
		else
		{
			TraversalStack.push_back(Node.Left);
			TraversalStack.push_back(Node.Right);
		}
	}

	Statistics.Candidates = OutIds.size();
	Statistics.TimeMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Start).count();
}

// 회전된 박스와 겹치는 Primitive Bounds를 찾아 해당 ID를 반환한다.
void FSceneBVH::OBBQuery(const FOBB& Box, TArray<uint64>& OutIds, FSceneBVHQueryStatistics& Statistics)
{
	Prepare();
	const auto Start = std::chrono::steady_clock::now();

	OutIds.clear();
	Statistics = {};
	Statistics.QueryCount = 1;
	TraversalStack.clear();

	if (Root != -1 && Box.IsValid())
	{
		TraversalStack.push_back(Root);
	}

	while (!TraversalStack.empty())
	{
		const FNode& Node = Nodes[TraversalStack.back()];
		TraversalStack.pop_back();

		++Statistics.BoundsTests;

		if (!Box.Intersect(Node.Bounds))
		{
			continue;
		}

		if (Node.Entry != -1)
		{
			OutIds.push_back(Entries[Node.Entry].PrimitiveId);
		}
		else
		{
			TraversalStack.push_back(Node.Left);
			TraversalStack.push_back(Node.Right);
		}
	}

	Statistics.Candidates = OutIds.size();
	Statistics.TimeMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Start).count();
}

// Bounds를 Extent만큼 늘려 Start~End 선분과 겹치는 ID를 반환하며, 메시 표면이나 실제 Sphere 충돌은 검사하지 않는다.
void FSceneBVH::SegmentQuery(const FVector& Start, const FVector& End, const FVector& Extent, TArray<uint64>& OutIds, FSceneBVHQueryStatistics& Statistics)
{
	check(Extent.X >= 0.0f && Extent.Y >= 0.0f && Extent.Z >= 0.0f);

	Prepare();
	const auto QueryStart = std::chrono::steady_clock::now();

	OutIds.clear();
	Statistics = {};
	Statistics.QueryCount = 1;
	TraversalStack.clear();

	const FRay Segment(Start, End - Start); // 방향 길이를 유지해 t=0이 Start, t=1이 End가 되도록 한다.

	if (Root != -1)
	{
		TraversalStack.push_back(Root);
	}

	while (!TraversalStack.empty())
	{
		const FNode& Node = Nodes[TraversalStack.back()];
		TraversalStack.pop_back();

		const FAABB Expanded(Node.Bounds.Min - Extent, Node.Bounds.Max + Extent);
		float Near = 0.0f;
		float Far = 0.0f;
		++Statistics.BoundsTests;

		if (!Expanded.IntersectRay(Segment, Near, Far) || Near > 1.0f)
		{
			continue;
		}

		if (Node.Entry != -1)
		{
			OutIds.push_back(Entries[Node.Entry].PrimitiveId);
		}
		else
		{
			TraversalStack.push_back(Node.Left);
			TraversalStack.push_back(Node.Right);
		}
	}

	Statistics.Candidates = OutIds.size();
	Statistics.TimeMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - QueryStart).count();
}
