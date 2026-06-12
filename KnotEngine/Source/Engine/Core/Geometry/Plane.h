#pragma once

#include <cmath>
#include "Core/Math/Vector.h"
#include "Core/Math/Utils.h"

struct FPlane
{
public:
    FVector Normal;
    float D{ 0.0f };

public:
    FPlane()
        : Normal(0.0f, 0.0f, 0.0f), D(0.0f) {}

    FPlane(const FVector& InNormal, float InD)
        : Normal(InNormal), D(InD) {}

    FPlane(const FVector& InNormal, const FVector& PointOnPlane)
        : Normal(InNormal), D(-FVector::Dot(InNormal, PointOnPlane)) {}

    FPlane(const FVector& PointA, const FVector& PointB, const FVector& PointC)
    {
        const FVector Edge1 = PointB - PointA;
        const FVector Edge2 = PointC - PointA;
        Normal = FVector::Cross(Edge1, Edge2);

        if (!Normalize(KMath::Epsilon))
        {
            Normal = FVector(0.0f, 0.0f, 0.0f);
            D = 0.0f;
            return;
        }

        D = -FVector::Dot(Normal, PointA);
    }

    float GetSignedDistance(const FVector& Point) const
    {
        const XMVector Dot = DirectX::XMVector3Dot(Normal.ToXMVector(), Point.ToXMVector());
        return DirectX::XMVectorGetX(Dot) + D;
    }

    float GetDistance(const FVector& Point) const
    {
        return std::fabs(GetSignedDistance(Point));
    }

    bool Normalize(float Tolerance = KMath::Epsilon)
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

        return true;
    }

    FPlane GetNormalized(float Tolerance = KMath::Epsilon) const
    {
        FPlane Result(*this);
        Result.Normalize(Tolerance);
        return Result;
    }

    void Flip()
    {
        Normal = -Normal;
        D = -D;
    }

    bool IsValid(float Tolerance = KMath::Epsilon) const
    {
        const XMVector NormalV = Normal.ToXMVector();
        const XMVector LengthV = DirectX::XMVector3Length(NormalV);
        return DirectX::XMVectorGetX(LengthV) > Tolerance;
    }
};
