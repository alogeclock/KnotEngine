#pragma once

#include <algorithm>
#include <cfloat>

#include "Core/Math/Vector.h"
#include "Core/Math/Matrix.h"
#include "Core/Math/Utils.h"
#include "Core/Geometry/Ray.h"

struct FAABB
{
	FVector Min;
	FVector Max;

	FAABB() { Reset(); }
	FAABB(const FVector& InMin, const FVector& InMax) : Min(InMin), Max(InMax) {}

	void Reset()
	{
		Min = FVector(FLT_MAX, FLT_MAX, FLT_MAX);
		Max = FVector(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	}

	bool IsValid() const { return Min.X <= Max.X && Min.Y <= Max.Y && Min.Z <= Max.Z; }

	void Expand(const FVector& Point)
	{
		const XMVector PointV = Point.ToXMVector();
		const XMVector MinV = Min.ToXMVector();
		const XMVector MaxV = Max.ToXMVector();

		Min = FVector(DirectX::XMVectorMin(MinV, PointV));
		Max = FVector(DirectX::XMVectorMax(MaxV, PointV));
	}

	void Merge(const FAABB& Other)
	{
		if (!Other.IsValid())
		{
			return;
		}

		Expand(Other.Min);
		Expand(Other.Max);
	}

	inline FVector GetCenter() const
	{
		const XMVector Center = DirectX::XMVectorScale(DirectX::XMVectorAdd(Min.ToXMVector(), Max.ToXMVector()), 0.5f);
		return FVector(Center);
	}

	inline FVector GetExtent() const
	{
		const XMVector Extent = DirectX::XMVectorScale(DirectX::XMVectorSubtract(Max.ToXMVector(), Min.ToXMVector()), 0.5f);
		return FVector(Extent);
	}

	bool IntersectRay(const FRay& Ray, float& OutT) const
	{
		if (!IsValid())
		{
			return false;
		}

		float TMin = 0.0f;
		float TMax = FLT_MAX;

		for (int Axis = 0; Axis < 3; Axis++)
		{
			const float Origin = (&Ray.Origin.X)[Axis];
			const float Direction = (&Ray.Direction.X)[Axis];
			const float BoxMin = Min[Axis];
			const float BoxMax = Max[Axis];

			if (std::fabs(Direction) < KMath::Epsilon)
			{
				if (Origin < Min[Axis] || Origin > Max[Axis])
				{
					return false;
				}
				continue;
			}

			const float InvD = (&Ray.InvD.X)[Axis];
			float       T1 = (BoxMin - Origin) * InvD;
			float       T2 = (BoxMax - Origin) * InvD;

			if (T1 > T2)
			{
				std::swap(T1, T2);
			}

			TMin = std::max(TMin, T1);
			TMax = std::min(TMax, T2);

			if (TMin > TMax)
			{
				return false;
			}
		}

		OutT = TMin;

		return true;
	}

	bool IntersectRay(const FRay& Ray, float& OutTMin, float& OutTMax) const
	{
		// Early Exit
		float t1 = (Min.X - Ray.Origin.X) / Ray.Direction.X;
		float t2 = (Max.X - Ray.Origin.X) / Ray.Direction.X;
		OutTMin = std::min(t1, t2);
		OutTMax = std::max(t1, t2);

		for (int i = 1; i < 3; ++i)
		{ // Y, Z 축 반복
			t1 = (Min[i] - Ray.Origin[i]) / Ray.Direction[i];
			t2 = (Max[i] - Ray.Origin[i]) / Ray.Direction[i];
			OutTMin = std::max(OutTMin, std::min(t1, t2));
			OutTMax = std::min(OutTMax, std::max(t1, t2));
		}

		return OutTMax >= OutTMin && OutTMax >= 0.0f;
	}

	static FAABB TransformAABB(const FAABB& InLocalAABB, const FMatrix& InMatrix)
	{
		const FVector& Min = InLocalAABB.Min;
		const FVector& Max = InLocalAABB.Max;

		const FVector Corners[8] = {
			FVector(Min.X, Min.Y, Min.Z), FVector(Max.X, Min.Y, Min.Z), FVector(Min.X, Max.Y, Min.Z),
			FVector(Max.X, Max.Y, Min.Z), FVector(Min.X, Min.Y, Max.Z), FVector(Max.X, Min.Y, Max.Z),
			FVector(Min.X, Max.Y, Max.Z), FVector(Max.X, Max.Y, Max.Z),
		};

		FVector NewMin(FLT_MAX, FLT_MAX, FLT_MAX);
		FVector NewMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

		for (const FVector& Corner : Corners)
		{
			const FVector P = InMatrix.TransformPosition(Corner);

			NewMin.X = (P.X < NewMin.X) ? P.X : NewMin.X;
			NewMin.Y = (P.Y < NewMin.Y) ? P.Y : NewMin.Y;
			NewMin.Z = (P.Z < NewMin.Z) ? P.Z : NewMin.Z;

			NewMax.X = (P.X > NewMax.X) ? P.X : NewMax.X;
			NewMax.Y = (P.Y > NewMax.Y) ? P.Y : NewMax.Y;
			NewMax.Z = (P.Z > NewMax.Z) ? P.Z : NewMax.Z;
		}

		return FAABB(NewMin, NewMax);
	}

	void ExpandToInclude(const FAABB& Other)
	{
		const XMVector MinV = Min.ToXMVector();
		const XMVector MaxV = Max.ToXMVector();
		const XMVector OtherMinV = Other.Min.ToXMVector();
		const XMVector OtherMaxV = Other.Max.ToXMVector();

		Min = FVector(DirectX::XMVectorMin(MinV, OtherMinV));
		Max = FVector(DirectX::XMVectorMax(MaxV, OtherMaxV));
	}

	bool NearlyEqualAABB(const FAABB& Other) const { return Min.Equals(Other.Min) && Max.Equals(Other.Max); }

	static bool NearlyEqualAABB(const FAABB& A, const FAABB& B) { return A.Min.Equals(B.Min) && A.Max.Equals(B.Max); }
};
