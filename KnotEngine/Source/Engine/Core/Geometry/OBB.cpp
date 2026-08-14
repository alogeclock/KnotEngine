
#include "Core/Geometry/OBB.h"

#include <cmath>

#include "Core/Geometry/AABB.h"
#include "Core/Math/Matrix.h"

// FOBB 객체를 초기화합니다.
FOBB::FOBB() = default;

// FOBB 객체를 초기화합니다.
FOBB::FOBB(const FVector& InCenter, const FVector& InExtent, const FQuat& InRotation)
	: Center(InCenter), Extent(InExtent), Rotation(InRotation)
{
}

// FOBB 객체를 초기화합니다.
FOBB::FOBB(const FVector& InCenter, const FVector& InExtent, const FMatrix& InMatrix)
	: Center(InCenter), Extent(InExtent), Rotation(InMatrix)
{
}

// 객체를 초기 상태로 되돌립니다.
void FOBB::Reset()
{
	Center = FVector::ZeroVector;
	Extent = FVector::ZeroVector;
	Rotation = FQuat::Identity;
}

// IsValid 조건을 검사합니다.
bool FOBB::IsValid() const
{
	return Extent.X > 0.0f && Extent.Y > 0.0f && Extent.Z > 0.0f;
}

// 교차 여부와 필요한 결과를 계산합니다.
bool FOBB::IntersectOBBAABB(const FAABB& AABB) const
{
	const FVector AABBCenter = AABB.GetCenter();
	const FVector AABBExtent = AABB.GetExtent();
	const FVector OBBAxes[3] = { Rotation.GetForward().GetSafeNormal(), Rotation.GetRight().GetSafeNormal(), Rotation.GetUp().GetSafeNormal() };
	const FVector AABBAxes[3] = { FVector::ForwardVector, FVector::RightVector, FVector::UpVector };

	float R[3][3];
	float AbsR[3][3];
	for (int I = 0; I < 3; ++I)
	{
		for (int J = 0; J < 3; ++J)
		{
			R[I][J] = OBBAxes[I] | AABBAxes[J];
			AbsR[I][J] = std::abs(R[I][J]) + 1e-6f;
		}
	}

	const FVector Translation = AABBCenter - Center;
	const FVector T(
		Translation | OBBAxes[0],
		Translation | OBBAxes[1],
		Translation | OBBAxes[2]);
	float RadiusA = 0.0f;
	float RadiusB = 0.0f;

	for (int I = 0; I < 3; ++I)
	{
		RadiusA = Extent[I];
		RadiusB = AABBExtent.X * AbsR[I][0] + AABBExtent.Y * AbsR[I][1] + AABBExtent.Z * AbsR[I][2];
		if (std::abs(T[I]) > RadiusA + RadiusB)
		{
			return false;
		}
	}

	for (int J = 0; J < 3; ++J)
	{
		RadiusA = Extent.X * AbsR[0][J] + Extent.Y * AbsR[1][J] + Extent.Z * AbsR[2][J];
		RadiusB = AABBExtent[J];
		const float Distance = std::abs(T.X * R[0][J] + T.Y * R[1][J] + T.Z * R[2][J]);
		if (Distance > RadiusA + RadiusB)
		{
			return false;
		}
	}

	for (int I = 0; I < 3; ++I)
	{
		for (int J = 0; J < 3; ++J)
		{
			RadiusA = Extent[(I + 1) % 3] * AbsR[(I + 2) % 3][J] + Extent[(I + 2) % 3] * AbsR[(I + 1) % 3][J];
			RadiusB = AABBExtent[(J + 1) % 3] * AbsR[I][(J + 2) % 3] + AABBExtent[(J + 2) % 3] * AbsR[I][(J + 1) % 3];
			const float Distance = std::abs(T[(I + 2) % 3] * R[(I + 1) % 3][J] - T[(I + 1) % 3] * R[(I + 2) % 3][J]);
			if (Distance > RadiusA + RadiusB)
			{
				return false;
			}
		}
	}

	return true;
}

// 입력값으로 FromAABB 결과를 생성합니다.
FOBB FOBB::FromAABB(const FAABB& InAABB, const FMatrix& InTransform)
{
	FOBB Result;
	Result.Center = InAABB.GetCenter();
	const FVector Scale(
		InTransform.GetScaledAxis(EAxis::X).Size(),
		InTransform.GetScaledAxis(EAxis::Y).Size(),
		InTransform.GetScaledAxis(EAxis::Z).Size());
	Result.Extent = InAABB.GetExtent() * Scale;
	Result.Rotation = FQuat(InTransform);
	return Result;
}
