#pragma once

#include "Core/Math/Math.h"
#include "Core/Math/Vector.h"

struct FPlane
{
    // 멤버 변수 (Member Variables)
    FVector Normal;
    float D = 0.0f;

    // 생성자 (Constructors)
    FPlane();
    FPlane(const FVector& InNormal, float InD);
    FPlane(const FVector& InNormal, const FVector& PointOnPlane);
    FPlane(const FVector& PointA, const FVector& PointB, const FVector& PointC);

    // 인스턴스 유틸리티 함수 (Instance Utility Functions)
    float GetSignedDistance(const FVector& Point) const;
    float GetDistance(const FVector& Point) const;
    bool Normalize(float Tolerance = KMath::Epsilon);
    FPlane GetNormalized(float Tolerance = KMath::Epsilon) const;

    void Flip();
    bool IsValid(float Tolerance = KMath::Epsilon) const;
};
