#pragma once

#include <cmath>
#include "Math/Vector.h"
#include "Math/Utils.h"

struct FPlane
{
public:
	FVector Normal;
	FVector AbsNormal;
	float   D{ 0.0f };

public:
	FPlane() : Normal(0.0f, 0.0f, 0.0f), AbsNormal(0.0f, 0.0f, 0.0f), D(0.0f)
	{
	}

	FPlane(const FVector& InNormal, float InD) : Normal(InNormal), AbsNormal(0.0f, 0.0f, 0.0f), D(InD)
	{
		UpdateAbsNormal();
	}

	FPlane(const FVector& InNormal, const FVector& PointOnPlane) : Normal(InNormal), AbsNormal(0.0f, 0.0f, 0.0f), D(-FVector::DotProduct(InNormal, PointOnPlane))
	{
		UpdateAbsNormal();
	}

	FPlane(const FVector& PointA, const FVector& PointB, const FVector& PointC)
	{
		const FVector Edge1 = PointB - PointA;
		const FVector Edge2 = PointC - PointA;
		Normal = FVector::CrossProduct(Edge1, Edge2);
		AbsNormal = FVector(0.0f, 0.0f, 0.0f);

		if (!Normalize(MathUtil::Epsilon))
		{
			Normal = FVector(0.0f, 0.0f, 0.0f);
			AbsNormal = FVector(0.0f, 0.0f, 0.0f);
			D = 0.0f;
			return;
		}

		D = -FVector::DotProduct(Normal, PointA);
		UpdateAbsNormal();
	}

	float GetSignedDistanceToPoint(const FVector& Point) const
	{
		const XMVector Dot = DirectX::XMVector3Dot(Normal.ToXMVector(), Point.ToXMVector());
		return DirectX::XMVectorGetX(Dot) + D;
	}

	float GetAbsDistanceToPoint(const FVector& Point) const
	{
		return std::fabs(GetSignedDistanceToPoint(Point));
	}

	bool Normalize(float Tolerance = MathUtil::Epsilon)
	{
		const XMVector NormalV = Normal.ToXMVector();
		const XMVector LengthV = DirectX::XMVector3Length(NormalV);
		const float Length = DirectX::XMVectorGetX(LengthV);
		if (Length <= Tolerance)
		{
			return false;
		}

		const float InvLength = 1.0f / Length;
		const XMVector Scale = DirectX::XMVectorReplicate(InvLength);
		Normal = FVector(DirectX::XMVectorMultiply(NormalV, Scale));
		D *= InvLength;
		UpdateAbsNormal();
		return true;
	}

	FPlane GetNormalized(float Tolerance = MathUtil::Epsilon) const
	{
		FPlane Result(*this);
		Result.Normalize(Tolerance);
		return Result;
	}

	void Flip()
	{
		Normal = -Normal;
		D = -D;
		UpdateAbsNormal();
	}

	bool IsValid(float Tolerance = MathUtil::Epsilon) const
	{
		const XMVector NormalV = Normal.ToXMVector();
		const XMVector LengthV = DirectX::XMVector3Length(NormalV);
		return DirectX::XMVectorGetX(LengthV) > Tolerance;
	}

private:
	void UpdateAbsNormal()
	{
		AbsNormal = FVector(std::fabs(Normal.X), std::fabs(Normal.Y), std::fabs(Normal.Z));
	}
};
