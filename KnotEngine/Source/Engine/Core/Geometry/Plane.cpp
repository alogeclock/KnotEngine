
#include "Core/Geometry/Plane.h"

#include <cmath>

// FPlane 객체를 초기화합니다.
FPlane::FPlane()
    : Normal(0.0f, 0.0f, 0.0f), D(0.0f)
{
}

// FPlane 객체를 초기화합니다.
FPlane::FPlane(const FVector& InNormal, float InD)
    : Normal(InNormal), D(InD)
{
}

// FPlane 객체를 초기화합니다.
FPlane::FPlane(const FVector& InNormal, const FVector& PointOnPlane)
    : Normal(InNormal), D(-(InNormal | PointOnPlane))
{
}

// FPlane 객체를 초기화합니다.
FPlane::FPlane(const FVector& PointA, const FVector& PointB, const FVector& PointC)
{
    const FVector Edge1 = PointB - PointA;
    const FVector Edge2 = PointC - PointA;
    Normal = Edge1 ^ Edge2;

    if (!Normalize(KMath::Epsilon))
    {
        Normal = FVector::ZeroVector;
        D = 0.0f;
        return;
    }

    D = -(Normal | PointA);
}

// SignedDistance 값을 반환합니다.
float FPlane::GetSignedDistance(const FVector& Point) const
{
    return (Normal | Point) + D;
}

// Distance 값을 반환합니다.
float FPlane::GetDistance(const FVector& Point) const
{
    return std::fabs(GetSignedDistance(Point));
}

// 값을 안전하게 정규화합니다.
bool FPlane::Normalize(float Tolerance)
{
    const float Length = Normal.Size();
    if (Length <= Tolerance)
    {
        return false;
    }

    const float InvLength = 1.0f / Length;
    Normal *= InvLength;
    D *= InvLength;
    return true;
}

// Normalized 값을 반환합니다.
FPlane FPlane::GetNormalized(float Tolerance) const
{
    FPlane Result(*this);
    Result.Normalize(Tolerance);
    return Result;
}

// 평면의 방향을 반전합니다.
void FPlane::Flip()
{
    Normal = -Normal;
    D = -D;
}

// IsValid 조건을 검사합니다.
bool FPlane::IsValid(float Tolerance) const
{
    return Normal.Size() > Tolerance;
}
