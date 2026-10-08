#include "Asset/MeshOptimizer.h"

// meshoptimizer의 Cache·Overdraw 설계를 참고했다. MIT License: Docs/Licenses/meshoptimizer.md.
#include <algorithm>
#include <cstring>

// Section 안의 삼각형을 FIFO 캐시와 남은 인접 Face 수 기준으로 재배치한다.
void FMeshOptimizer::OptimizeVertexCache(std::span<uint32> Indices, SIZE_T VertexCount)
{
	if (Indices.empty())
	{
		return;
	}

	static constexpr uint32 CacheSize = 16;
	const uint32 TriangleCount = static_cast<uint32>(Indices.size() / 3);
	TArray<uint32> Counts(VertexCount, 0);
	TArray<uint32> Offsets(VertexCount + 1, 0);

	// 1. Index의 사용 수와 Prefix Sum을 계산하여 정점별 인접 삼각형을 연속 배열로 구성한다.
	for (const uint32 Index : Indices)
	{
		++Counts[Index];
	}

	for (SIZE_T Index = 0; Index < VertexCount; ++Index)
	{
		Offsets[Index + 1] = Offsets[Index] + Counts[Index];
	}

	TArray<uint32> Cursor = Offsets;
	TArray<uint32> Adjacency(Indices.size());
	// 현재 정점의 인접 삼각형 목록에 Triangle 번호를 저장한 뒤 Cursor를 1 증가한다.
	for (uint32 Triangle = 0; Triangle < TriangleCount; ++Triangle)
	{
		for (uint32 Corner = 0; Corner < 3; ++Corner)
		{
			const uint32 Vertex = Indices[Triangle * 3 + Corner];
			const uint32 Index = Cursor[Vertex]++;
			Adjacency[Index] = Triangle;
		}
	}

	TArray<uint8> Emitted(TriangleCount, 0);
	TArray<uint64> Timestamps(VertexCount, 0);
	TArray<uint32> DeadEnds;
	TArray<uint32> Output;

	Output.reserve(Indices.size());
	DeadEnds.reserve(Indices.size());

	uint64 Timestamp = CacheSize + 1;
	uint32 InputCursor = 0;
	uint32 Current = Indices.front();
	TArray<uint32> Candidates;

	// 2. 현재 정점의 삼각형을 출력하고 다음에 처리할 정점을 선택한다.
	while (Current != UINT32_MAX)
	{
		Candidates.clear();

		// 현재 정점의 인접 삼각형을 FIFO 순서로 출력하고 각 정점의 남은 인접 삼각형 수를 감소시킨다.
		for (uint32 Entry = Offsets[Current]; Entry < Offsets[Current + 1]; ++Entry)
		{
			const uint32 Triangle = Adjacency[Entry];
			
			if (Emitted[Triangle])
			{
				continue;
			}

			Emitted[Triangle] = 1;

			for (uint32 Corner = 0; Corner < 3; ++Corner)
			{
				const uint32 Vertex = Indices[Triangle * 3 + Corner];
				Output.push_back(Vertex);
				--Counts[Vertex];
				DeadEnds.push_back(Vertex);
				Candidates.push_back(Vertex);
				if (Timestamp - Timestamps[Vertex] > CacheSize)
				{
					Timestamps[Vertex] = Timestamp++;
				}
			}
		}

		Current = UINT32_MAX;
		uint64 BestPriority = 0;

		// 남은 인접 삼각형 수와 FIFO Age와 남은 참조 수를 기준으로 다음 정점을 선택한다.
		// 휴리스틱 전략으로 정확한 미래 캐시 미스 수를 계산하지 못한다.
		for (const uint32 Vertex : Candidates)
		{
			if (Counts[Vertex] == 0)
			{
				continue;
			}

			const uint64 Age = Timestamp - Timestamps[Vertex];
			const uint64 Priority = 2ull * Counts[Vertex] + Age <= CacheSize ? Age : 0;

			if (Current == UINT32_MAX || Priority > BestPriority)
			{
				Current = Vertex;
				BestPriority = Priority;
			}
		}

		// 실패 시 DeadEnd 목록에서 남은 인접 삼각형이 있는 정점을 선택한다.
		while (Current == UINT32_MAX && !DeadEnds.empty())
		{
			const uint32 Vertex = DeadEnds.back();
			DeadEnds.pop_back();
			if (Counts[Vertex] > 0)
			{
				Current = Vertex;
			}
		}

		// 실패 시 남은 인접 삼각형이 있는 정점을 순차적으로 선택한다.
		while (Current == UINT32_MAX && InputCursor < VertexCount)
		{
			if (Counts[InputCursor] > 0)
			{
				Current = InputCursor;
			}
			++InputCursor;
		}
	}

	check(Output.size() == Indices.size());

	// 캐시 미스 수 비교 후 최적화 결과가 더 나을 때만 원본 Index Buffer를 덮어쓴다.
	if (CountCacheMisses(Output, VertexCount) <= CountCacheMisses(Indices, VertexCount))
	{
		std::copy(Output.begin(), Output.end(), Indices.begin());
	}
}

// 16정점 FIFO 모델에서 캐시 미스 수를 계산해 삼각형 재배치의 회귀를 검사한다.
uint64 FMeshOptimizer::CountCacheMisses(std::span<const uint32> Indices, SIZE_T VertexCount)
{
	static constexpr uint64 CacheSize = 16;
	TArray<uint64> Timestamps(VertexCount, 0);

	uint64 Timestamp = CacheSize + 1;
	uint64 Misses = 0;

	for (const uint32 Index : Indices)
	{
		if (Timestamp - Timestamps[Index] > CacheSize)
		{
			Timestamps[Index] = Timestamp++;
			++Misses;
		}
	}

	return Misses;
}

// Cache 순서의 작은 Cluster를 외향성 기준으로 정렬하며 FIFO 비용 증가를 5% 이내로 제한한다.
void FMeshOptimizer::OptimizeOverdraw(std::span<uint32> Indices, const TArray<FStaticMeshVertex>& Vertices)
{
	if (Indices.size() < 192)
	{
		return;
	}

	FVector Center = FVector::ZeroVector;

	for (const uint32 Index : Indices)
	{
		Center += Vertices[Index].Position;
	}

	Center *= 1.0f / static_cast<float>(Indices.size());
	const uint64 CacheMisses = CountCacheMisses(Indices, Vertices.size());

	// 외향성 순서로 Cluster를 정렬하고 캐시 비용이 허용 범위인 결과만 채택한다.
	for (const uint32 ClusterTriangles : { 32u, 64u, 128u, 256u })
	{
		TArray<FCluster> Clusters;
		for (uint32 First = 0; First < Indices.size(); First += ClusterTriangles * 3)
		{
			const uint32 Count = std::min(ClusterTriangles * 3, static_cast<uint32>(Indices.size()) - First);
			FVector Centroid = FVector::ZeroVector;
			FVector Normal = FVector::ZeroVector;
			float Area = 0.0f;

			for (uint32 Index = First; Index < First + Count; Index += 3)
			{
				const FVector& A = Vertices[Indices[Index]].Position;
				const FVector& B = Vertices[Indices[Index + 1]].Position;
				const FVector& C = Vertices[Indices[Index + 2]].Position;
				const FVector Cross = (B - A) ^ (C - A);
				const float Weight = Cross.Size();
				Centroid += (A + B + C) * (Weight / 3.0f);
				Normal += Cross;
				Area += Weight;
			}

			if (Area > 0.0f)
			{
				Centroid *= 1.0f / Area;
			}

			Clusters.push_back({ First, Count, (Centroid - Center) | Normal.GetSafeNormal() });
		}

		std::stable_sort(Clusters.begin(), Clusters.end(), [](const FCluster& A, const FCluster& B)
		{
			return A.Score > B.Score;
		});

		TArray<uint32> Output;
		Output.reserve(Indices.size());

		for (const FCluster& Cluster : Clusters)
		{
			Output.insert(Output.end(), Indices.begin() + Cluster.FirstIndex, Indices.begin() + Cluster.FirstIndex + Cluster.IndexCount);
		}

		if (CountCacheMisses(Output, Vertices.size()) <= CacheMisses * 1.05)
		{
			std::copy(Output.begin(), Output.end(), Indices.begin());
			return;
		}
	}
}

// 동일 속성을 통합한 뒤 Cache → Overdraw → Fetch를 수행하고 전후 통계를 반환한다.
FMeshOptimizationStatistics FMeshOptimizer::Optimize(
	TArray<FStaticMeshVertex>& Vertices,
	TArray<uint32>& Indices,
	const TArray<FStaticMeshSection>& Sections,
	bool bReorderTriangles)
{
	FMeshOptimizationStatistics Statistics;
	Statistics.VertexCountBefore = Vertices.size();

	const auto CountSectionMisses = [&]()
	{
		uint64 Misses = 0;
		if (Sections.empty())
		{
			return CountCacheMisses(Indices, Vertices.size());
		}
		for (const FStaticMeshSection& Section : Sections)
		{
			Misses += CountCacheMisses(std::span<const uint32>(Indices).subspan(Section.FirstIndex, Section.IndexCount), Vertices.size());
		}
		return Misses;
	};

	check(Indices.size() % 3 == 0);
	for (const uint32 Index : Indices)
	{
		check(Index < Vertices.size());
	}

	Statistics.CacheMissesBefore = CountSectionMisses();

	// Position만으로 합치지 않는다. UV, Normal, Tangent와 Handedness가 같은 Render Vertex만 통합한다.
	TMap<uint64, TArray<uint32>> Buckets;
	TArray<uint32> Canonical(Vertices.size(), UINT32_MAX);
	for (uint32& Index : Indices)
	{
		uint32& Destination = Canonical[Index];
		if (Destination == UINT32_MAX)
		{
			const uint8* Bytes = reinterpret_cast<const uint8*>(&Vertices[Index]);
			uint64 Hash = 14695981039346656037ull;
			for (SIZE_T Byte = 0; Byte < sizeof(FStaticMeshVertex); ++Byte)
			{
				Hash = (Hash ^ Bytes[Byte]) * 1099511628211ull;
			}
			TArray<uint32>& Bucket = Buckets[Hash];
			Destination = Index;
			for (const uint32 Other : Bucket)
			{
				if (std::memcmp(&Vertices[Index], &Vertices[Other], sizeof(FStaticMeshVertex)) == 0)
				{
					Destination = Other;
					break;
				}
			}
			if (Destination == Index)
			{
				Bucket.push_back(Index);
			}
		}
		Index = Destination;
	}

	// Material 경계를 유지하며 Section별 Vertex Cache 순서를 최적화한다.
	if (bReorderTriangles && Sections.empty())
	{
		OptimizeVertexCache(Indices, Vertices.size());
	}

	else if (bReorderTriangles)
	{
		for (const FStaticMeshSection& Section : Sections)
		{
			check(Section.FirstIndex % 3 == 0 && Section.IndexCount % 3 == 0);
			check(static_cast<SIZE_T>(Section.FirstIndex) + Section.IndexCount <= Indices.size());
			OptimizeVertexCache(std::span<uint32>(Indices).subspan(Section.FirstIndex, Section.IndexCount), Vertices.size());
		}
	}

	Statistics.CacheMissesAfterCache = CountSectionMisses();

	// 순서 보존이 필요한 메시를 제외하고 Section별 Overdraw 순서를 최적화한다.
	if (bReorderTriangles && Sections.empty())
	{
		OptimizeOverdraw(Indices, Vertices);
	}

	else if (bReorderTriangles)
	{
		for (const FStaticMeshSection& Section : Sections)
		{
			OptimizeOverdraw(std::span<uint32>(Indices).subspan(Section.FirstIndex, Section.IndexCount), Vertices);
		}
	}

	Statistics.CacheMissesAfterOverdraw = CountSectionMisses();

	// 최초 참조 순서로 정점을 배치하고 사용하지 않는 정점을 제거한다.
	TArray<uint32> Remap(Vertices.size(), UINT32_MAX);
	TArray<FStaticMeshVertex> Output;
	Output.reserve(Vertices.size());

	for (uint32& Index : Indices)
	{
		uint32& Destination = Remap[Index];
		if (Destination == UINT32_MAX)
		{
			Destination = static_cast<uint32>(Output.size());
			Output.push_back(Vertices[Index]);
		}
		Index = Destination;
	}

	Vertices = std::move(Output);
	Statistics.VertexCountAfter = Vertices.size();
	return Statistics;
}
