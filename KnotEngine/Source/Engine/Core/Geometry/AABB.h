#pragma once

#include "Core/Math/Math.h"
#include "Core/Math/Vector.h"

struct FMatrix;
struct FRay;

struct FAABB
{
    FVector Min;
    FVector Max;

    FAABB() noexcept;
    FAABB(const FVector& InMin, const FVector& InMax) noexcept;

    void Reset() noexcept;
    bool IsValid() const noexcept;

    void Expand(const FVector& Point) noexcept;
    void Merge(const FAABB& Other) noexcept;

    FVector GetCenter() const noexcept;
    FVector GetExtent() const noexcept;

    bool IntersectRay(const FRay& Ray, float& OutTMin, float& OutTMax) const noexcept;
    bool Equals(const FAABB& Other, float Tolerance = KMath::Epsilon) const noexcept;

    FAABB Transform(const FMatrix& Matrix) const noexcept;
};
