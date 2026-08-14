#pragma once

#include "Core/Math/Quat.h"
#include "Core/Math/Vector.h"

struct FMatrix;

struct FTransform
{
public:
    static const FTransform Identity;

    FQuat Rotation = FQuat::Identity;
    FVector Translation = FVector::ZeroVector;
    FVector Scale3D = FVector::OneVector;

public:
    FTransform() noexcept = default;
    FTransform(const FQuat& InRotation, const FVector& InTranslation = FVector::ZeroVector,
               const FVector& InScale3D = FVector::OneVector) noexcept;

    FTransform operator*(const FTransform& Other) const noexcept;
    FTransform& operator*=(const FTransform& Other) noexcept;

    FVector TransformPosition(const FVector& Position) const noexcept;
    FVector TransformVector(const FVector& Vector) const noexcept;
    FVector InverseTransformPosition(const FVector& Position) const noexcept;
    FVector InverseTransformVector(const FVector& Vector) const noexcept;

    FMatrix ToMatrix() const noexcept;
    FTransform Inverse() const noexcept;
};
