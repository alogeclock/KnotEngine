#include "Asset/MeshSimplifier.h"

#include <algorithm>
#include <cmath>
#include <queue>

// 다른 Quadric의 누적 오차를 현재 Quadric에 합산한다.
FMeshSimplifier::FQuadric& FMeshSimplifier::FQuadric::operator+=(const FQuadric& Other)
{
	for (int32 Index = 0; Index < 10; ++Index)
	{
		Values[Index] += Other.Values[Index];
	}
	return *this;
}

// 주어진 위치에서 Quadric Error 값을 계산한다.
float FMeshSimplifier::FQuadric::Evaluate(const FVector& Position) const
{
	const float X = Position.X;
	const float Y = Position.Y;
	const float Z = Position.Z;
	return Values[0] * X * X + 2.0f * Values[1] * X * Y + 2.0f * Values[2] * X * Z + 2.0f * Values[3] * X +
	       Values[4] * Y * Y + 2.0f * Values[5] * Y * Z + 2.0f * Values[6] * Y + Values[7] * Z * Z + 2.0f * Values[8] * Z + Values[9];
}

// 삼각형 평면이 만드는 오차를 면적 Weight와 함께 Quadric에 누적한다.
void FMeshSimplifier::FQuadric::AddPlane(const FVector& Normal, float Distance, float Weight)
{
	const float Plane[4] = { Normal.X, Normal.Y, Normal.Z, Distance };
	static constexpr int32 Rows[10] = { 0, 0, 0, 0, 1, 1, 1, 2, 2, 3 };
	static constexpr int32 Columns[10] = { 0, 1, 2, 3, 1, 2, 3, 2, 3, 3 };
	for (int32 Index = 0; Index < 10; ++Index)
	{
		Values[Index] += Plane[Rows[Index]] * Plane[Columns[Index]] * Weight;
	}
}

FMeshSimplifier::FMeshSimplifier(const FMeshSimplificationDesc& Desc)
	: Vertices(Desc.Vertices), SourceIndices(Desc.Indices), SourceSections(Desc.Sections), TriangleCount(static_cast<int32>(Desc.Indices.size() / 3))
{
}

// 하나의 Edge Collapse 흐름을 이어가며 요청된 삼각형 비율마다 LOD Snapshot을 생성한다.
TArray<FStaticMeshLODBuildData> FMeshSimplifier::GenerateLODs(const FMeshSimplificationDesc& Desc)
{
	TArray<FStaticMeshLODBuildData> LODs;
	if (Desc.Vertices.empty() || Desc.Indices.size() < 3 || Desc.Indices.size() % 3 != 0 || Desc.TriangleRatios.empty())
	{
		return LODs;
	}
	float PreviousRatio = 1.0f;
	for (const float Ratio : Desc.TriangleRatios)
	{
		if (!std::isfinite(Ratio) || Ratio <= 0.0f || Ratio >= PreviousRatio)
		{
			return LODs;
		}
		PreviousRatio = Ratio;
	}

	// Render Vertex를 위치 기준 Topological Vertex로 묶고 각 평면의 Quadric과 Edge 연결 관계를 구축한다.
	FMeshSimplifier Simplifier(Desc);
	Simplifier.BuildTopology();
	Simplifier.BuildQuadricsAndEdges();

	// 모든 Edge의 Collapse Error를 계산해 가장 손실이 작은 후보부터 꺼낼 수 있도록 구성한다.
	const int32 SourceTriangleCount = Simplifier.TriangleCount;
	SIZE_T TargetLODIndex = 0;
	int32 TargetTriangleCount = std::max(1, static_cast<int32>(SourceTriangleCount * Desc.TriangleRatios[TargetLODIndex]));
	std::priority_queue<FCollapseCandidate> Candidates;
	for (const FIndexEdge& Edge : Simplifier.Edges)
	{
		Candidates.push(Simplifier.BuildCollapseCandidate(Edge.A, Edge.B));
	}

	// 최저 Error Edge를 반복해서 Collapse하고 목표 삼각형 수에 도달할 때마다 현재 Mesh를 LOD로 저장한다.
	TArray<uint32> Neighbors;
	while (!Candidates.empty() && TargetLODIndex < Desc.TriangleRatios.size())
	{
		const FCollapseCandidate Candidate = Candidates.top();
		Candidates.pop();
		const FTopologicalVertex& VertexA = Simplifier.TopologicalVertices[Candidate.Edge.A];
		const FTopologicalVertex& VertexB = Simplifier.TopologicalVertices[Candidate.Edge.B];
		if (VertexA.bDeleted || VertexB.bDeleted || Candidate.VersionA != VertexA.Version || Candidate.VersionB != VertexB.Version)
		{
			continue;
		}

		// 주변 Collapse로 상태가 바뀐 후보는 현재 Topology 기준으로 다시 평가한다.
		const FCollapseCandidate CurrentCandidate = Simplifier.BuildCollapseCandidate(Candidate.Edge.A, Candidate.Edge.B);
		if (!std::isfinite(CurrentCandidate.Error) || CurrentCandidate.Error >= (std::numeric_limits<float>::max)())
		{
			continue;
		}
		if (CurrentCandidate.Error > Candidate.Error + 0.0001f)
		{
			Candidates.push(CurrentCandidate);
			continue;
		}

		Neighbors.clear();
		if (!Simplifier.Collapse(CurrentCandidate, Neighbors))
		{
			continue;
		}
		for (const uint32 Neighbor : Neighbors)
		{
			Candidates.push(Simplifier.BuildCollapseCandidate(CurrentCandidate.Edge.A, Neighbor));
		}

		if (Simplifier.TriangleCount <= TargetTriangleCount)
		{
			LODs.push_back(Simplifier.BuildCurrentLOD());
			++TargetLODIndex;
			if (TargetLODIndex == Desc.TriangleRatios.size())
			{
				break;
			}
			TargetTriangleCount = std::max(1, static_cast<int32>(SourceTriangleCount * Desc.TriangleRatios[TargetLODIndex]));
		}
	}
	return LODs;
}

// 위치가 같은 Render Vertex를 하나의 Topological Vertex로 묶되 UV Seam 등의 원본 정점은 따로 보존한다.
void FMeshSimplifier::BuildTopology()
{
	static constexpr float PositionTolerance = 0.001f;
	static constexpr float InverseTolerance = 1.0f / PositionTolerance;
	TMap<uint64, TArray<uint32>> VerticesByCell;
	TArray<uint32> RenderToTopological;
	RenderToTopological.resize(Vertices.size());
	TopologicalIndices.resize(SourceIndices.size());

	for (uint32 RenderVertexIndex = 0; RenderVertexIndex < Vertices.size(); ++RenderVertexIndex)
	{
		const FVector& Position = Vertices[RenderVertexIndex].Position;
		const int32 X = static_cast<int32>(std::floor(Position.X * InverseTolerance));
		const int32 Y = static_cast<int32>(std::floor(Position.Y * InverseTolerance));
		const int32 Z = static_cast<int32>(std::floor(Position.Z * InverseTolerance));
		const uint64 Key = (static_cast<uint64>(static_cast<uint32>(X)) & 0x1fffff) << 42 |
		                   (static_cast<uint64>(static_cast<uint32>(Y)) & 0x1fffff) << 21 |
		                   (static_cast<uint64>(static_cast<uint32>(Z)) & 0x1fffff);
		uint32 TopologicalIndex = (std::numeric_limits<uint32>::max)();
		for (const uint32 CandidateIndex : VerticesByCell[Key])
		{
			if (FVector::DistSquared(TopologicalVertices[CandidateIndex].Position, Position) < PositionTolerance * PositionTolerance)
			{
				TopologicalIndex = CandidateIndex;
				break;
			}
		}
		if (TopologicalIndex == (std::numeric_limits<uint32>::max)())
		{
			TopologicalIndex = static_cast<uint32>(TopologicalVertices.size());
			TopologicalVertices.push_back({ Position });
			VerticesByCell[Key].push_back(TopologicalIndex);
		}
		TopologicalVertices[TopologicalIndex].RenderVertices.push_back(RenderVertexIndex);
		RenderToTopological[RenderVertexIndex] = TopologicalIndex;
	}

	for (SIZE_T Index = 0; Index < SourceIndices.size(); ++Index)
	{
		const uint32 RenderIndex = SourceIndices[Index];
		check(RenderIndex < Vertices.size());
		TopologicalIndices[Index] = RenderToTopological[RenderIndex];
	}
}

// 각 삼각형 평면의 Quadric Error와 Edge 인접 관계를 만들고 경계 Edge의 정점을 표시한다.
void FMeshSimplifier::BuildQuadricsAndEdges()
{
	const uint32 SourceTriangleCount = static_cast<uint32>(SourceIndices.size() / 3);
	AliveTriangles.assign(SourceTriangleCount, true);
	TMap<FIndexEdge, uint32, FIndexEdgeHasher> EdgeUsage;
	for (uint32 TriangleIndex = 0; TriangleIndex < SourceTriangleCount; ++TriangleIndex)
	{
		const uint32 A = TopologicalIndices[TriangleIndex * 3];
		const uint32 B = TopologicalIndices[TriangleIndex * 3 + 1];
		const uint32 C = TopologicalIndices[TriangleIndex * 3 + 2];
		if (A == B || B == C || A == C)
		{
			AliveTriangles[TriangleIndex] = false;
			continue;
		}
		TopologicalVertices[A].Triangles.push_back(TriangleIndex);
		TopologicalVertices[B].Triangles.push_back(TriangleIndex);
		TopologicalVertices[C].Triangles.push_back(TriangleIndex);
		const FIndexEdge TriangleEdges[] = { FIndexEdge(A, B).Canonical(), FIndexEdge(B, C).Canonical(), FIndexEdge(C, A).Canonical() };
		for (const FIndexEdge& Edge : TriangleEdges)
		{
			Edges.insert(Edge);
			++EdgeUsage[Edge];
		}

		const FVector& PositionA = TopologicalVertices[A].Position;
		const FVector Cross = (TopologicalVertices[B].Position - PositionA) ^ (TopologicalVertices[C].Position - PositionA);
		const float DoubleArea = Cross.Size();
		if (DoubleArea <= KMath::Epsilon)
		{
			continue;
		}
		const FVector Normal = Cross / DoubleArea;
		const float Distance = -(Normal | PositionA);
		const float Area = DoubleArea * 0.5f;
		TopologicalVertices[A].Quadric.AddPlane(Normal, Distance, Area);
		TopologicalVertices[B].Quadric.AddPlane(Normal, Distance, Area);
		TopologicalVertices[C].Quadric.AddPlane(Normal, Distance, Area);
	}
	for (const auto& [Edge, Usage] : EdgeUsage)
	{
		if (Usage == 1)
		{
			TopologicalVertices[Edge.A].bBoundary = true;
			TopologicalVertices[Edge.B].bBoundary = true;
		}
	}
}

// 두 Topological Vertex를 합칠 최적 위치와 그 위치에서 발생하는 Quadric Error를 계산한다.
FMeshSimplifier::FCollapseCandidate FMeshSimplifier::BuildCollapseCandidate(uint32 VertexA, uint32 VertexB) const
{
	FCollapseCandidate Result;
	Result.Edge = FIndexEdge(VertexA, VertexB).Canonical();
	const FTopologicalVertex& A = TopologicalVertices[Result.Edge.A];
	const FTopologicalVertex& B = TopologicalVertices[Result.Edge.B];
	Result.VersionA = A.Version;
	Result.VersionB = B.Version;
	if (A.bDeleted || B.bDeleted || A.bBoundary || B.bBoundary)
	{
		return Result;
	}

	FQuadric Quadric = A.Quadric;
	Quadric += B.Quadric;
	// 합쳐진 Quadric에서 Error가 최소가 되는 Collapse 위치를 계산한다.
	const float A00 = Quadric.Values[0];
	const float A01 = Quadric.Values[1];
	const float A02 = Quadric.Values[2];
	const float A11 = Quadric.Values[4];
	const float A12 = Quadric.Values[5];
	const float A22 = Quadric.Values[7];
	const float Determinant = A00 * (A11 * A22 - A12 * A12) - A01 * (A01 * A22 - A12 * A02) + A02 * (A01 * A12 - A11 * A02);
	if (std::fabs(Determinant) > 0.000001f)
	{
		const FVector Right(-Quadric.Values[3], -Quadric.Values[6], -Quadric.Values[8]);
		Result.Position.X = ((A11 * A22 - A12 * A12) * Right.X + (A02 * A12 - A01 * A22) * Right.Y + (A01 * A12 - A02 * A11) * Right.Z) / Determinant;
		Result.Position.Y = ((A02 * A12 - A01 * A22) * Right.X + (A00 * A22 - A02 * A02) * Right.Y + (A01 * A02 - A00 * A12) * Right.Z) / Determinant;
		Result.Position.Z = ((A01 * A12 - A02 * A11) * Right.X + (A01 * A02 - A00 * A12) * Right.Y + (A00 * A11 - A01 * A01) * Right.Z) / Determinant;
		Result.Error = Quadric.Evaluate(Result.Position);
	}
	else
	{
		// 위치를 계산할 수 없다면 두 끝점과 중점 중 Error가 가장 작은 위치를 사용한다.
		const FVector Middle = (A.Position + B.Position) * 0.5f;
		const float ErrorA = Quadric.Evaluate(A.Position);
		const float ErrorB = Quadric.Evaluate(B.Position);
		const float ErrorMiddle = Quadric.Evaluate(Middle);
		Result.Position = ErrorA <= ErrorB && ErrorA <= ErrorMiddle ? A.Position : ErrorB <= ErrorMiddle ? B.Position : Middle;
		Result.Error = std::min({ ErrorA, ErrorB, ErrorMiddle });
	}
	if (WouldInvertTriangle(Result.Edge.A, Result.Edge.B, Result.Position))
	{
		Result.Error = (std::numeric_limits<float>::max)();
	}
	return Result;
}

// Edge Collapse 이후 인접 삼각형이 뒤집히거나 면적을 잃는지 검사한다.
bool FMeshSimplifier::WouldInvertTriangle(uint32 VertexA, uint32 VertexB, const FVector& Position) const
{
	const auto CheckTriangles = [&](const TArray<uint32>& Triangles)
	{
		for (const uint32 TriangleIndex : Triangles)
		{
			if (!AliveTriangles[TriangleIndex])
			{
				continue;
			}
			const uint32 IndicesValue[3] = {
				TopologicalIndices[TriangleIndex * 3],
				TopologicalIndices[TriangleIndex * 3 + 1],
				TopologicalIndices[TriangleIndex * 3 + 2]
			};
			const bool bHasA = IndicesValue[0] == VertexA || IndicesValue[1] == VertexA || IndicesValue[2] == VertexA;
			const bool bHasB = IndicesValue[0] == VertexB || IndicesValue[1] == VertexB || IndicesValue[2] == VertexB;
			if (bHasA && bHasB)
			{
				continue;
			}
			const FVector Old[3] = {
				TopologicalVertices[IndicesValue[0]].Position,
				TopologicalVertices[IndicesValue[1]].Position,
				TopologicalVertices[IndicesValue[2]].Position
			};
			const FVector New[3] = {
				IndicesValue[0] == VertexA || IndicesValue[0] == VertexB ? Position : Old[0],
				IndicesValue[1] == VertexA || IndicesValue[1] == VertexB ? Position : Old[1],
				IndicesValue[2] == VertexA || IndicesValue[2] == VertexB ? Position : Old[2]
			};
			const FVector OldNormal = (Old[1] - Old[0]) ^ (Old[2] - Old[0]);
			const FVector NewNormal = (New[1] - New[0]) ^ (New[2] - New[0]);
			if (NewNormal.SizeSquared() <= 0.0000000000000001f || (OldNormal | NewNormal) <= 0.0f)
			{
				return true;
			}
		}
		return false;
	};
	return CheckTriangles(TopologicalVertices[VertexA].Triangles) || CheckTriangles(TopologicalVertices[VertexB].Triangles);
}

// 선택한 Edge를 하나의 Vertex로 합치고 영향을 받은 삼각형과 이웃 Vertex를 갱신한다.
bool FMeshSimplifier::Collapse(const FCollapseCandidate& Candidate, TArray<uint32>& Neighbors)
{
	const uint32 VertexA = Candidate.Edge.A;
	const uint32 VertexB = Candidate.Edge.B;
	FTopologicalVertex& A = TopologicalVertices[VertexA];
	FTopologicalVertex& B = TopologicalVertices[VertexB];
	if (A.bDeleted || B.bDeleted)
	{
		return false;
	}

	A.Position = Candidate.Position;
	A.Quadric += B.Quadric;
	++A.Version;
	B.bDeleted = true;
	++B.Version;
	for (const uint32 RenderVertex : A.RenderVertices)
	{
		Vertices[RenderVertex].Position = Candidate.Position;
	}
	for (const uint32 RenderVertex : B.RenderVertices)
	{
		Vertices[RenderVertex].Position = Candidate.Position;
		A.RenderVertices.push_back(RenderVertex);
	}

	// 두 Vertex의 인접 삼각형을 합친 뒤 축퇴된 삼각형을 제거하고 재평가할 이웃을 수집한다.
	TSet<uint32> UniqueTriangles(A.Triangles.begin(), A.Triangles.end());
	UniqueTriangles.insert(B.Triangles.begin(), B.Triangles.end());
	A.Triangles.assign(UniqueTriangles.begin(), UniqueTriangles.end());
	TSet<uint32> UniqueNeighbors;
	for (const uint32 TriangleIndex : A.Triangles)
	{
		if (!AliveTriangles[TriangleIndex])
		{
			continue;
		}
		uint32* Triangle = &TopologicalIndices[TriangleIndex * 3];
		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			if (Triangle[Corner] == VertexB)
			{
				Triangle[Corner] = VertexA;
			}
		}
		if (Triangle[0] == Triangle[1] || Triangle[1] == Triangle[2] || Triangle[0] == Triangle[2])
		{
			AliveTriangles[TriangleIndex] = false;
			--TriangleCount;
			continue;
		}
		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			if (Triangle[Corner] != VertexA && !TopologicalVertices[Triangle[Corner]].bDeleted)
			{
				UniqueNeighbors.insert(Triangle[Corner]);
			}
		}
	}
	Neighbors.assign(UniqueNeighbors.begin(), UniqueNeighbors.end());
	return true;
}

// 현재 살아 있는 삼각형만 모아 Section을 보존한 직렬화용 LOD 데이터를 생성한다.
FStaticMeshLODBuildData FMeshSimplifier::BuildCurrentLOD() const
{
	FStaticMeshLODBuildData Result;
	TMap<uint32, uint32> RemappedVertices;
	// 살아 있는 삼각형이 실제로 참조하는 Render Vertex만 새 Index Buffer에 압축한다.
	const auto AppendIndex = [&](uint32 SourceIndex)
	{
		const auto Iterator = RemappedVertices.find(SourceIndex);
		if (Iterator != RemappedVertices.end())
		{
			Result.Indices.push_back(Iterator->second);
			return;
		}
		const uint32 NewIndex = static_cast<uint32>(Result.Vertices.size());
		RemappedVertices.emplace(SourceIndex, NewIndex);
		Result.Vertices.push_back(Vertices[SourceIndex]);
		Result.Indices.push_back(NewIndex);
	};

	const auto AppendSection = [&](const FStaticMeshSection& SourceSection)
	{
		const uint32 FirstIndex = static_cast<uint32>(Result.Indices.size());
		const uint32 EndIndex = std::min(SourceSection.FirstIndex + SourceSection.IndexCount, static_cast<uint32>(SourceIndices.size()));
		for (uint32 Index = SourceSection.FirstIndex; Index + 2 < EndIndex; Index += 3)
		{
			const uint32 TriangleIndex = Index / 3;
			if (!AliveTriangles[TriangleIndex])
			{
				continue;
			}
			AppendIndex(SourceIndices[Index]);
			AppendIndex(SourceIndices[Index + 1]);
			AppendIndex(SourceIndices[Index + 2]);
		}
		const uint32 IndexCount = static_cast<uint32>(Result.Indices.size()) - FirstIndex;
		if (IndexCount > 0)
		{
			Result.Sections.push_back({ FirstIndex, IndexCount, SourceSection.MaterialIndex });
		}
	};
	if (SourceSections.empty())
	{
		AppendSection({ 0, static_cast<uint32>(SourceIndices.size()), 0 });
	}
	else
	{
		for (const FStaticMeshSection& SourceSection : SourceSections)
		{
			AppendSection(SourceSection);
		}
	}
	return Result;
}
