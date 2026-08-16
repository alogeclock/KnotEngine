
#include "Core/Geometry/Frustum.h"

#include "Core/Geometry/AABB.h"
#include "Core/Math/Matrix.h"
#include "Core/Math/Math.h"

#include <cmath>

// 입력값을 기준으로 상태를 갱신합니다.
void FFrustum::UpdateFromCamera(const FMatrix& View, const FMatrix& Projection)
{
	UpdateFromCamera(View * Projection);
}

// 입력값을 기준으로 상태를 갱신합니다.
void FFrustum::UpdateFromCamera(const FMatrix& ViewProjection)
{
	const float(&M)[4][4] = ViewProjection.M;
	const FVector4 C0(M[0][0], M[1][0], M[2][0], M[3][0]);
	const FVector4 C1(M[0][1], M[1][1], M[2][1], M[3][1]);
	const FVector4 C2(M[0][2], M[1][2], M[2][2], M[3][2]);
	const FVector4 C3(M[0][3], M[1][3], M[2][3], M[3][3]);
	const FVector4 PlaneValues[6] = {
		C0 + C3,
		C3 - C0,
		C1 + C3,
		C3 - C1,
		C3 - C2,
		C2
	};

	for (int Index = 0; Index < 6; ++Index)
	{
		const FVector4& Values = PlaneValues[Index];
		Planes[Index] = FPlane(FVector(Values.X, Values.Y, Values.Z), Values.W);
		Planes[Index].Normalize(KMath::Epsilon);
	}

	const FVector TestPoint = ViewProjection.GetInverse().TransformPosition(FVector(0.0f, 0.0f, 0.5f));
	for (FPlane& Plane : Planes)
	{
		if (Plane.GetSignedDistance(TestPoint) < 0.0f)
		{
			Plane.Flip();
		}
	}

	for (const FPlane& Plane : Planes)
	{
		check(Plane.GetSignedDistance(TestPoint) >= -KMath::Epsilon);
	}
}

// 교차 여부와 필요한 결과를 계산합니다.
FFrustum::EFrustumIntersectResult FFrustum::Intersects(const FAABB& Box) const
{
	const FVector Center = Box.GetCenter();
	const FVector Extent = Box.GetExtent();
	bool bAllInside = true;
	for (const FPlane& Plane : Planes)
	{
		const float Radius = std::fabs(Plane.Normal.X * Extent.X) +
		                     std::fabs(Plane.Normal.Y * Extent.Y) +
		                     std::fabs(Plane.Normal.Z * Extent.Z);
		const float Distance = Plane.GetSignedDistance(Center);
		if (Distance + Radius < 0.0f)
		{
			return EFrustumIntersectResult::Outside;
		}
		if (Distance - Radius < 0.0f)
		{
			bAllInside = false;
		}
	}

	return bAllInside ? EFrustumIntersectResult::Inside : EFrustumIntersectResult::Intersect;
}

// Contains 조건을 검사합니다.
bool FFrustum::Contains(const FVector& Point) const
{
	for (const FPlane& Plane : Planes)
	{
		if (Plane.GetSignedDistance(Point) < 0.0f)
		{
			return false;
		}
	}
	return true;
}
