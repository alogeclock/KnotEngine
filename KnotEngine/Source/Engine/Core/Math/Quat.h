#pragma once

#include "Core/Math/Math.h"
#include "Core/Math/Vector.h"

struct FMatrix;
struct FRotator;

struct FQuat
{
public:
    // 멤버 변수 (Member Variables)
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;
    float W = 1.0f;

    static const FQuat Identity;

public:
    // 생성자 (Constructors)
    constexpr FQuat() noexcept = default;
    constexpr FQuat(float InX, float InY, float InZ, float InW) noexcept
        : X(InX), Y(InY), Z(InZ), W(InW)
    {
    }
    FQuat(const FVector& Axis, float AngleRad) noexcept;
    explicit FQuat(const FRotator& Rotator) noexcept;
    explicit FQuat(const FMatrix& Matrix) noexcept;

    // 비교 및 일반 사칙 연산자 (Comparison and Basic Math Operators)
    bool operator==(const FQuat& Other) const noexcept;
    bool operator!=(const FQuat& Other) const noexcept;
    FQuat operator-() const noexcept;
    FQuat operator+(const FQuat& Other) const noexcept;
    FQuat operator-(const FQuat& Other) const noexcept;
    FQuat operator*(float Scalar) const noexcept;
    FQuat operator/(float Scalar) const noexcept;
    FQuat operator*(const FQuat& Other) const noexcept;
    FVector operator*(const FVector& Vector) const noexcept;
    float operator|(const FQuat& Other) const noexcept;

    // 복합 대입 연산자 (Compound Assignment Operators)
    FQuat& operator+=(const FQuat& Other) noexcept;
    FQuat& operator-=(const FQuat& Other) noexcept;
    FQuat& operator*=(float Scalar) noexcept;
    FQuat& operator/=(float Scalar) noexcept;
    FQuat& operator*=(const FQuat& Other) noexcept;

    // 인스턴스 유틸리티 함수 (Instance Utility Functions)
    bool Equals(const FQuat& Other, float Tolerance = KMath::Epsilon) const noexcept;
    float SizeSquared() const noexcept;
    void Normalize(float Tolerance = KMath::Epsilon) noexcept;
    FQuat GetNormalized(float Tolerance = KMath::Epsilon) const noexcept;
    FQuat Inverse(float Tolerance = KMath::Epsilon) const noexcept;

    FVector RotateVector(const FVector& Vector) const noexcept;
    FVector UnrotateVector(const FVector& Vector) const noexcept;

    FVector GetForward() const noexcept;
    FVector GetRight() const noexcept;
    FVector GetUp() const noexcept;

    FRotator Rotator() const noexcept;
    FMatrix ToMatrix() const noexcept;

    // 공용 쿼터니언 계산기 (Static Quaternion Functions)
    static FQuat Slerp(const FQuat& A, const FQuat& B, float Alpha) noexcept;

private:
    // 내부 헬퍼 함수 (Private Helper Functions)
    static FQuat FromRotationMatrix(const FMatrix& Matrix) noexcept;
};

// 전역 산술 연산자 (Global Arithmetic Operators)
FQuat operator*(float Scalar, const FQuat& Quat) noexcept;
