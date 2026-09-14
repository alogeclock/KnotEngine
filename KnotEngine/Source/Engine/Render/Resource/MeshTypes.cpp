#include "Render/Resource/MeshTypes.h"

#include <cmath>

// Engine 기본 도형의 CPU 정점과 인덱스를 생성한다. GPU 업로드는 Resource Manager가 처음 요청받을 때 수행한다.
std::shared_ptr<FGeometryMesh> FGeometryMesh::Create(EGeometryMeshType MeshType)
{
	TArray<FGeometryVertex> Vertices;
	TArray<uint32> Indices;

	switch (MeshType)
	{
	case EGeometryMeshType::Quad:
		Vertices = {
			{ FVector(-1.0f, -1.0f, 0.0f), PackRGBA(204, 204, 204) },
			{ FVector(1.0f, -1.0f, 0.0f), PackRGBA(204, 204, 204) },
			{ FVector(1.0f, 1.0f, 0.0f), PackRGBA(204, 204, 204) },
			{ FVector(-1.0f, 1.0f, 0.0f), PackRGBA(204, 204, 204) },
		};
		Indices = { 0, 1, 2, 0, 2, 3, 2, 1, 0, 3, 2, 0 };
		break;

	case EGeometryMeshType::Sphere:
	{
		static constexpr uint32 LatitudeSegments = 16;
		static constexpr uint32 LongitudeSegments = 24;
		const auto ToColorChannel = [](float Value)
		{
			return static_cast<uint8>(KMath::Clamp((Value * 0.5f + 0.5f) * 255.0f, 0.0f, 255.0f));
		};
		Vertices.reserve((LatitudeSegments + 1) * (LongitudeSegments + 1));
		Indices.reserve(LatitudeSegments * LongitudeSegments * 6);

		for (uint32 LatitudeIndex = 0; LatitudeIndex <= LatitudeSegments; ++LatitudeIndex)
		{
			const float Latitude = -KMath::HalfPi + KMath::Pi * static_cast<float>(LatitudeIndex) / static_cast<float>(LatitudeSegments);
			const float RingRadius = std::cos(Latitude);
			const float Z = std::sin(Latitude);
			for (uint32 LongitudeIndex = 0; LongitudeIndex <= LongitudeSegments; ++LongitudeIndex)
			{
				const float Longitude = KMath::Tau * static_cast<float>(LongitudeIndex) / static_cast<float>(LongitudeSegments);
				const FVector Position(RingRadius * std::cos(Longitude), RingRadius * std::sin(Longitude), Z);
				const uint32 Color = PackRGBA(ToColorChannel(Position.X), ToColorChannel(Position.Y), ToColorChannel(Position.Z));
				Vertices.push_back({ Position, Color });
			}
		}

		const uint32 RingVertexCount = LongitudeSegments + 1;
		for (uint32 LatitudeIndex = 0; LatitudeIndex < LatitudeSegments; ++LatitudeIndex)
		{
			for (uint32 LongitudeIndex = 0; LongitudeIndex < LongitudeSegments; ++LongitudeIndex)
			{
				const uint32 BottomLeft = LatitudeIndex * RingVertexCount + LongitudeIndex;
				const uint32 BottomRight = BottomLeft + 1;
				const uint32 TopLeft = BottomLeft + RingVertexCount;
				const uint32 TopRight = TopLeft + 1;
				Indices.insert(Indices.end(), { BottomLeft, BottomRight, TopRight, BottomLeft, TopRight, TopLeft });
			}
		}
		break;
	}

	case EGeometryMeshType::Cube:
		Vertices = {
			{ FVector(-1.0f, -1.0f, -1.0f), PackRGBA(255, 51, 51) },
			{ FVector(-1.0f, 1.0f, -1.0f), PackRGBA(255, 51, 51) },
			{ FVector(1.0f, 1.0f, -1.0f), PackRGBA(255, 51, 51) },
			{ FVector(1.0f, -1.0f, -1.0f), PackRGBA(255, 51, 51) },
			{ FVector(-1.0f, -1.0f, 1.0f), PackRGBA(51, 255, 77) },
			{ FVector(1.0f, -1.0f, 1.0f), PackRGBA(51, 255, 77) },
			{ FVector(1.0f, 1.0f, 1.0f), PackRGBA(51, 255, 77) },
			{ FVector(-1.0f, 1.0f, 1.0f), PackRGBA(51, 255, 77) },
			{ FVector(-1.0f, -1.0f, 1.0f), PackRGBA(51, 115, 255) },
			{ FVector(-1.0f, 1.0f, 1.0f), PackRGBA(51, 115, 255) },
			{ FVector(-1.0f, 1.0f, -1.0f), PackRGBA(51, 115, 255) },
			{ FVector(-1.0f, -1.0f, -1.0f), PackRGBA(51, 115, 255) },
			{ FVector(1.0f, -1.0f, -1.0f), PackRGBA(255, 217, 51) },
			{ FVector(1.0f, 1.0f, -1.0f), PackRGBA(255, 217, 51) },
			{ FVector(1.0f, 1.0f, 1.0f), PackRGBA(255, 217, 51) },
			{ FVector(1.0f, -1.0f, 1.0f), PackRGBA(255, 217, 51) },
			{ FVector(-1.0f, 1.0f, -1.0f), PackRGBA(230, 230, 51) },
			{ FVector(-1.0f, 1.0f, 1.0f), PackRGBA(230, 230, 51) },
			{ FVector(1.0f, 1.0f, 1.0f), PackRGBA(230, 230, 51) },
			{ FVector(1.0f, 1.0f, -1.0f), PackRGBA(230, 230, 51) },
			{ FVector(-1.0f, -1.0f, 1.0f), PackRGBA(217, 64, 255) },
			{ FVector(-1.0f, -1.0f, -1.0f), PackRGBA(217, 64, 255) },
			{ FVector(1.0f, -1.0f, -1.0f), PackRGBA(217, 64, 255) },
			{ FVector(1.0f, -1.0f, 1.0f), PackRGBA(217, 64, 255) },
		};
		Indices = { 0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7, 8, 9, 10, 8, 10, 11, 12, 13, 14, 12, 14, 15, 16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23, };
		break;
	}

	panic(!Vertices.empty() && !Indices.empty());
	auto Mesh = std::make_shared<FGeometryMesh>();
	Mesh->Initialize(Vertices, Indices);
	return Mesh;
}

// C 배열과 TArray 등 연속 메모리를 소유권 이전 없이 크기와 함께 받기 위해 span을 사용한다.
// 입력 데이터는 함수 안에서 CPU Mesh 배열로 복사하므로 span은 호출 중에만 유효하면 된다.
void FGeometryMesh::Initialize(std::span<const FGeometryVertex> InVertices, std::span<const uint32> InIndices)
{
	Release();

	Vertices.assign(InVertices.begin(), InVertices.end());
	Indices.assign(InIndices.begin(), InIndices.end());
	LocalBounds.Reset();
	for (const FGeometryVertex& Vertex : Vertices)
	{
		LocalBounds.Expand(Vertex.Position);
	}
}

// FGeometryMesh는 남긴 채, GPU에 업로드된 데이터를 해제한다.
void FGeometryMesh::Release()
{
	MeshBuffer.Release();
}
