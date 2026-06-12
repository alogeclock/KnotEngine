#pragma once

#include "Core/Math/Vector.h"
#include "Core/Math/Quat.h"
#include "Core/Math/Matrix.h"
#include "Core/Geometry/AABB.h"

struct FOBB
{
	FVector Center = { 0.f, 0.f, 0.f };
	FVector Extent = { 0.5f, 0.5f, 0.5f };
	FQuat Rotation = { 0.f, 0.f, 0.f, 1.f };

	FOBB()
		: Center(0.0f, 0.0f, 0.0f), Extent(0.0f, 0.0f, 0.0f), Rotation(0.0f, 0.0f, 0.0f, 1.0f)
	{
	}

	FOBB(const FVector& InCenter, const FVector& InExtent, const FQuat& InRotation)
		: Center(InCenter), Extent(InExtent), Rotation(InRotation)
	{
	}

	FOBB(const FVector& InCenter, const FVector& InExtent, const FMatrix& InMatrix)
		: Center(InCenter), Extent(InExtent), Rotation(InMatrix)
	{
	}

	void Reset()
	{
		Center = FVector::Zero();
		Extent = FVector::Zero();
		Rotation = FQuat::Identity;
	}

	bool IsValid() const
	{
		return Extent.X > 0.0f && Extent.Y > 0.0f && Extent.Z > 0.0f;
	}

	static FOBB FromAABB(const FAABB& InAABB, const FMatrix& InTransform)
	{
		FOBB Result;

		Result.Center = InAABB.GetCenter();

		FVector Scale;
		Scale.X = InTransform.GetScaledAxis(EAxis::X).Size();
		Scale.Y = InTransform.GetScaledAxis(EAxis::Y).Size();
		Scale.Z = InTransform.GetScaledAxis(EAxis::Z).Size();

		Result.Extent = InAABB.GetExtent() * Scale;

		Result.Rotation = FQuat(InTransform);

		return Result;
	}

	// Decal receiver narrow phase에서 OBB-AABB SAT 교차 판정에 사용됩니다.
	bool IntersectOBBAABB(const FAABB& AABB) const
	{
		const FVector AABBCenter = AABB.GetCenter();
		const FVector AABBExtent = AABB.GetExtent();
		const FVector OBBAxes[3] = { Rotation.GetForward().Normalized(), Rotation.GetRight().Normalized(), Rotation.GetUp().Normalized() };
		const FVector AABBAxes[3] = { FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f) };

		float R[3][3];
		float AbsR[3][3];
		for (int i = 0; i < 3; ++i)
		{
			for (int j = 0; j < 3; ++j)
			{
				R[i][j] = OBBAxes[i].Dot(AABBAxes[j]);
				AbsR[i][j] = std::abs(R[i][j]) + 1e-6f;
			}
		}

		const FVector Translation = AABBCenter - Center;
		const FVector T(
			Translation.Dot(OBBAxes[0]),
			Translation.Dot(OBBAxes[1]),
			Translation.Dot(OBBAxes[2]));

		float ra = 0.0f;
		float rb = 0.0f;

		for (int i = 0; i < 3; ++i)
		{
			ra = Extent.Data[i];
			rb = AABBExtent.X * AbsR[i][0] + AABBExtent.Y * AbsR[i][1] + AABBExtent.Z * AbsR[i][2];
			if (std::abs(T.Data[i]) > ra + rb)
			{
				return false;
			}
		}

		for (int j = 0; j < 3; ++j)
		{
			ra = Extent.X * AbsR[0][j] + Extent.Y * AbsR[1][j] + Extent.Z * AbsR[2][j];
			rb = AABBExtent.Data[j];
			const float Distance = std::abs(T.X * R[0][j] + T.Y * R[1][j] + T.Z * R[2][j]);
			if (Distance > ra + rb)
			{
				return false;
			}
		}

		for (int i = 0; i < 3; ++i)
		{
			for (int j = 0; j < 3; ++j)
			{
				ra = Extent.Data[(i + 1) % 3] * AbsR[(i + 2) % 3][j] + Extent.Data[(i + 2) % 3] * AbsR[(i + 1) % 3][j];
				rb = AABBExtent.Data[(j + 1) % 3] * AbsR[i][(j + 2) % 3] + AABBExtent.Data[(j + 2) % 3] * AbsR[i][(j + 1) % 3];
				const float Distance = std::abs(T.Data[(i + 2) % 3] * R[(i + 1) % 3][j] - T.Data[(i + 1) % 3] * R[(i + 2) % 3][j]);
				if (Distance > ra + rb)
				{
					return false;
				}
			}
		}

		return true;
	}
};
