
#include "Core/Geometry/Ray.h"

#include "Core/Math/Matrix.h"

#include <cmath>

// FRay 객체를 초기화합니다.
FRay::FRay(const FVector& InOrigin, const FVector& InDirection)
	: Origin(InOrigin)
{
	SetDirection(InDirection);
}

// Direction 값을 설정합니다.
void FRay::SetDirection(const FVector& NewDirection)
{
	Direction = NewDirection;

	// 0으로 나누면 IEEE 754에 의해 ±infinity가 되어 slab 테스트에서 평행 케이스를 자동 처리
	InvD.X = 1.0f / Direction.X;
	InvD.Y = 1.0f / Direction.Y;
	InvD.Z = 1.0f / Direction.Z;
}

// Moller-Trumbore 알고리즘으로 양면 Triangle과의 교차 거리를 계산한다.
bool FRay::Intersect(const FVector& Vertex0, const FVector& Vertex1, const FVector& Vertex2, float& OutDistance) const noexcept
{
	const FVector Edge1 = Vertex1 - Vertex0;
	const FVector Edge2 = Vertex2 - Vertex0;
	const FVector P = Direction ^ Edge2;
	const float Det = Edge1 | P;
	if (std::fabs(Det) <= KMath::Epsilon)
	{
		return false;
	}

	const float InvDet = 1.0f / Det;
	const FVector T = Origin - Vertex0;
	const float U = (T | P) * InvDet; // Vertex 1 방향의 무게중심 좌표
	if (U < 0.0f || U > 1.0f)
	{
		return false;
	}

	const FVector Q = T ^ Edge1;
	const float V = (Direction | Q) * InvDet;
	if (V < 0.0f || U + V > 1.0f)
	{
		return false;
	}

	const float Distance = (Edge2 | Q) * InvDet;
	if (Distance <= KMath::Epsilon)
	{
		return false;
	}

	OutDistance = Distance;
	return true;
}

// 입력값으로 BuildRay 결과를 생성합니다.
FRay FRay::BuildRay(float MouseX, float MouseY, const FMatrix& ViewProjection, float ViewportWidth, float ViewportHeight)
{
	if (ViewportWidth <= 0 || ViewportHeight <= 0)
	{
		return FRay{};
	}

	const float NDCX = 2.0f * MouseX / ViewportWidth - 1.0f;
	const float NDCY = 1.0f - 2.0f * MouseY / ViewportHeight;
	// Reversed-Z Projection은 Near=1, Far=0을 사용한다.
	const FVector NearPointNDC(NDCX, NDCY, 1.0f);
	const FVector FarPointNDC(NDCX, NDCY, 0.0f);
	const FMatrix InvViewProjection = ViewProjection.GetInverse();
	const FVector NearWorld = InvViewProjection.TransformPosition(NearPointNDC);
	const FVector FarWorld = InvViewProjection.TransformPosition(FarPointNDC);
	const FVector Direction = (FarWorld - NearWorld).GetSafeNormal();

	return FRay{ NearWorld, Direction };
}
