#pragma once

#include "Core/Math/Vector.h"

struct FMatrix;

struct FVector4
{
public:
    // 멤버 변수 (Member Variables)
    union
    {
        struct
        {
            float X;
            float Y;
            float Z;
            float W;
        };
        float Data[4];
    };

    static const FVector4 ZeroVector;

public:
    // 생성자 (Constructors)
    constexpr FVector4() noexcept : X(0.0f), Y(0.0f), Z(0.0f), W(0.0f) {}
    constexpr FVector4(float InX, float InY, float InZ, float InW = 0.0f) noexcept
        : X(InX), Y(InY), Z(InZ), W(InW)
    {
    }
    constexpr FVector4(const FVector& Vector, float InW = 0.0f) noexcept
        : X(Vector.X), Y(Vector.Y), Z(Vector.Z), W(InW)
    {
    }

    // 요소 접근 연산자 (Element Access Operators)
    float& operator[](int32 Index) noexcept;
    const float& operator[](int32 Index) const noexcept;

    // 비교 및 일반 사칙 연산자 (Comparison and Basic Math Operators)
    bool operator==(const FVector4& Other) const noexcept;
    bool operator!=(const FVector4& Other) const noexcept;
    FVector4 operator-() const noexcept;
    FVector4 operator+(const FVector4& Other) const noexcept;
    FVector4 operator-(const FVector4& Other) const noexcept;
    FVector4 operator*(float Scalar) const noexcept;
    FVector4 operator*(const FVector4& Other) const noexcept;
    FVector4 operator/(float Scalar) const noexcept;
    FVector4 operator*(const FMatrix& Matrix) const noexcept;
    float operator|(const FVector4& Other) const noexcept;

    // 복합 대입 연산자 (Compound Assignment Operators)
    FVector4& operator+=(const FVector4& Other) noexcept;
    FVector4& operator-=(const FVector4& Other) noexcept;
    FVector4& operator*=(float Scalar) noexcept;
    FVector4& operator/=(float Scalar) noexcept;

    // 인스턴스 유틸리티 함수 (Instance Utility Functions)
    bool Equals(const FVector4& Other, float Tolerance = KMath::Epsilon) const noexcept;
    bool IsZero() const noexcept;
    bool IsNearlyZero(float Tolerance = KMath::Epsilon) const noexcept;
    float SizeSquared() const noexcept;
    float Size() const noexcept;
    bool Normalize(float Tolerance = KMath::Epsilon) noexcept;
    FVector4 GetSafeNormal(float Tolerance = KMath::Epsilon) const noexcept;

    // 동차 좌표 유틸리티 함수 (Homogeneous Coordinate Utility Functions)
    bool IsPoint(float Tolerance = KMath::Epsilon) const noexcept;
    bool IsVector(float Tolerance = KMath::Epsilon) const noexcept;
    FVector ToVector3(float Tolerance = KMath::Epsilon) const noexcept;

    // 공용 동차 좌표 생성 함수 (Static Homogeneous Coordinate Functions)
    static constexpr FVector4 Point(float X = 0.0f, float Y = 0.0f, float Z = 0.0f) noexcept
    {
        return { X, Y, Z, 1.0f };
    }

    static constexpr FVector4 Vector(float X, float Y, float Z) noexcept
    {
        return { X, Y, Z, 0.0f };
    }
};
