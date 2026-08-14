#pragma once

#include "Core/Math/Math.h"
#include "Core/Math/Vector.h"

struct FQuat;

struct FRotator
{
public:
    float Pitch = 0.0f;
    float Yaw = 0.0f;
    float Roll = 0.0f;

    static const FRotator ZeroRotator;

public:
    constexpr FRotator() noexcept = default;
    constexpr FRotator(float InPitch, float InYaw, float InRoll) noexcept
        : Pitch(InPitch), Yaw(InYaw), Roll(InRoll)
    {
    }
    explicit FRotator(const FQuat& Quat) noexcept;

    bool operator==(const FRotator& Other) const noexcept;
    bool operator!=(const FRotator& Other) const noexcept;
    FRotator operator-() const noexcept;
    FRotator operator+(const FRotator& Other) const noexcept;
    FRotator operator-(const FRotator& Other) const noexcept;
    FRotator operator*(float Scalar) const noexcept;
    FRotator operator/(float Scalar) const noexcept;

    FRotator& operator+=(const FRotator& Other) noexcept;
    FRotator& operator-=(const FRotator& Other) noexcept;
    FRotator& operator*=(float Scalar) noexcept;
    FRotator& operator/=(float Scalar) noexcept;

    FVector Euler() const noexcept;
    FVector Vector() const noexcept;
    void Normalize() noexcept;
    FRotator GetNormalized() const noexcept;
    bool IsZero() const noexcept;
    bool IsNearlyZero(float Tolerance = KMath::Epsilon) const noexcept;
    bool Equals(const FRotator& Other, float Tolerance = KMath::Epsilon) const noexcept;

    FVector RotateVector(const FVector& Vector) const noexcept;
    FVector UnrotateVector(const FVector& Vector) const noexcept;
    FRotator GetInverse() const noexcept;
    FQuat Quaternion() const noexcept;

    static float NormalizeAxis(float AngleDegrees) noexcept;
    static FRotator MakeFromEuler(const FVector& EulerDegrees) noexcept;
};

FRotator operator*(float Scalar, const FRotator& Rotator) noexcept;
