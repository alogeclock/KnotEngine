#pragma once

#include "Core/CoreTypes.h"
#include "Core/Math/Math.h"

struct FVector
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
        };
        float Data[3];
    };

    static const FVector ZeroVector;
    static const FVector OneVector;
    static const FVector ForwardVector;
    static const FVector RightVector;
    static const FVector UpVector;

public:
    // 생성자 (Constructors)
    constexpr FVector() noexcept : X(0.0f), Y(0.0f), Z(0.0f) {}
    constexpr FVector(float InX, float InY, float InZ) noexcept : X(InX), Y(InY), Z(InZ) {}
    explicit FVector(const Float3& InFloat3) noexcept;

    // 요소 접근 연산자 (Element Access Operators)
    float& operator[](int32 Index) noexcept;
    const float& operator[](int32 Index) const noexcept;

    // 비교 및 일반 사칙 연산자 (Comparison and Basic Math Operators)
    bool operator==(const FVector& Other) const noexcept;
    bool operator!=(const FVector& Other) const noexcept;
    FVector operator-() const noexcept;
    FVector operator+(const FVector& Other) const noexcept;
    FVector operator-(const FVector& Other) const noexcept;
    FVector operator*(float Scalar) const noexcept;
    FVector operator*(const FVector& Other) const noexcept;
    FVector operator/(float Scalar) const noexcept;
    float operator|(const FVector& Other) const noexcept;
    FVector operator^(const FVector& Other) const noexcept;

    // 복합 대입 연산자 (Compound Assignment Operators)
    FVector& operator+=(const FVector& Other) noexcept;
    FVector& operator-=(const FVector& Other) noexcept;
    FVector& operator*=(float Scalar) noexcept;
    FVector& operator/=(float Scalar) noexcept;

    // 인스턴스 유틸리티 함수 (Instance Utility Functions)
    Float3 ToFloat3() const noexcept;
    bool Equals(const FVector& Other, float Tolerance = KMath::Epsilon) const noexcept;
    bool IsZero() const noexcept;
    bool IsNearlyZero(float Tolerance = KMath::Epsilon) const noexcept;
    float SizeSquared() const noexcept;
    float Size() const noexcept;
    bool Normalize(float Tolerance = KMath::Epsilon) noexcept;
    FVector GetSafeNormal(float Tolerance = KMath::Epsilon) const noexcept;

    // 공용 벡터 계산기 (Static Vector Functions)
    static float DistSquared(const FVector& A, const FVector& B) noexcept;
    static float Dist(const FVector& A, const FVector& B) noexcept;
};

namespace std
{
    template <>
    struct hash<FVector>
    {
        size_t operator()(const FVector& Vector) const noexcept;
    };
} // namespace std
