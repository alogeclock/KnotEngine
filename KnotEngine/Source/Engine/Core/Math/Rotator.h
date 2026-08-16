#pragma once

#include "Core/Math/Math.h"
#include "Core/Math/Vector.h"

struct FQuat;

struct FRotator
{
public:
    // 멤버 변수 (Member Variables)
    float Pitch = 0.0f;
    float Yaw = 0.0f;
    float Roll = 0.0f;

    static const FRotator ZeroRotator;

public:
    // 생성자 (Constructors)
    constexpr FRotator() noexcept = default;
    constexpr FRotator(float InPitch, float InYaw, float InRoll) noexcept
        : Pitch(InPitch), Yaw(InYaw), Roll(InRoll)
    {
    }
    explicit FRotator(const FQuat& Quat) noexcept;

    // 비교 및 일반 사칙 연산자 (Comparison and Basic Math Operators)
    bool operator==(const FRotator& Other) const noexcept;
    bool operator!=(const FRotator& Other) const noexcept;
    FRotator operator-() const noexcept;
    FRotator operator+(const FRotator& Other) const noexcept;
    FRotator operator-(const FRotator& Other) const noexcept;
    FRotator operator*(float Scalar) const noexcept;
    FRotator operator/(float Scalar) const noexcept;

    // 복합 대입 연산자 (Compound Assignment Operators)
    FRotator& operator+=(const FRotator& Other) noexcept;
    FRotator& operator-=(const FRotator& Other) noexcept;
    FRotator& operator*=(float Scalar) noexcept;
    FRotator& operator/=(float Scalar) noexcept;

    // 인스턴스 유틸리티 함수 (Instance Utility Functions)
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

    // 공용 회전 계산기 (Static Rotation Functions)
    static float NormalizeAxis(float AngleDegrees) noexcept;
    static FRotator MakeFromEuler(const FVector& EulerDegrees) noexcept;
};

// 전역 산술 연산자 (Global Arithmetic Operators)
FRotator operator*(float Scalar, const FRotator& Rotator) noexcept;
