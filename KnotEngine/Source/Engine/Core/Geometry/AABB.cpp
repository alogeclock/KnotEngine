#include "Core/Geometry/AABB.h"

#include "Core/Geometry/Ray.h"
#include "Core/Math/Matrix.h"

#include <algorithm>
#include <cmath>
#include <limits>

FAABB::FAABB() noexcept
{
	Reset();
}

FAABB::FAABB(const FVector& InMin, const FVector& InMax) noexcept
	: Min(InMin), Max(InMax)
{
}

void FAABB::Reset() noexcept
{
	const float MaxFloat = (std::numeric_limits<float>::max)();
	Min = FVector(MaxFloat, MaxFloat, MaxFloat);
	Max = FVector(-MaxFloat, -MaxFloat, -MaxFloat);
}

bool FAABB::IsValid() const noexcept
{
	return Min.X <= Max.X && Min.Y <= Max.Y && Min.Z <= Max.Z;
}

void FAABB::Expand(const FVector& Point) noexcept
{
	Min = FVector(std::min(Min.X, Point.X), std::min(Min.Y, Point.Y), std::min(Min.Z, Point.Z));
	Max = FVector(std::max(Max.X, Point.X), std::max(Max.Y, Point.Y), std::max(Max.Z, Point.Z));
}

void FAABB::Merge(const FAABB& Other) noexcept
{
	if (!Other.IsValid())
	{
		return;
	}

	Expand(Other.Min);
	Expand(Other.Max);
}

FVector FAABB::GetCenter() const noexcept
{
	return (Min + Max) * 0.5f;
}

FVector FAABB::GetExtent() const noexcept
{
	return (Max - Min) * 0.5f;
}

bool FAABB::IntersectRay(const FRay& Ray, float& OutTMin, float& OutTMax) const noexcept
{
	if (!IsValid())
	{
		return false;
	}

	OutTMin = 0.0f;
	OutTMax = (std::numeric_limits<float>::max)();

	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const float Origin = Ray.Origin[Axis];
		const float Direction = Ray.Direction[Axis];

		if (std::fabs(Direction) <= KMath::Epsilon)
		{
			if (Origin < Min[Axis] || Origin > Max[Axis])
			{
				return false;
			}
			continue;
		}

		float T1 = (Min[Axis] - Origin) * Ray.InvD[Axis];
		float T2 = (Max[Axis] - Origin) * Ray.InvD[Axis];
		if (T1 > T2)
		{
			std::swap(T1, T2);
		}

		OutTMin = std::max(OutTMin, T1);
		OutTMax = std::min(OutTMax, T2);
		if (OutTMin > OutTMax)
		{
			return false;
		}
	}

	return true;
}

bool FAABB::Equals(const FAABB& Other, float Tolerance) const noexcept
{
	return Min.Equals(Other.Min, Tolerance) && Max.Equals(Other.Max, Tolerance);
}

FAABB FAABB::Transform(const FMatrix& Matrix) const noexcept
{
	if (!IsValid())
	{
		return FAABB();
	}

	const FVector Corners[8] = {
		{ Min.X, Min.Y, Min.Z },
		{ Max.X, Min.Y, Min.Z },
		{ Min.X, Max.Y, Min.Z },
		{ Max.X, Max.Y, Min.Z },
		{ Min.X, Min.Y, Max.Z },
		{ Max.X, Min.Y, Max.Z },
		{ Min.X, Max.Y, Max.Z },
		{ Max.X, Max.Y, Max.Z },
	};

	FAABB Result;
	for (const FVector& Corner : Corners)
	{
		Result.Expand(Matrix.TransformPosition(Corner));
	}
	return Result;
}
