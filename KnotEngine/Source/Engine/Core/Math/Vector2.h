#pragma once

#include "Object/Reflection/ReflectionMacros.h"

#include <cmath>

#include "Core/CoreTypes.h"
#include "Core/Math/Math.h"

USTRUCT()
struct FVector2
{
	GENERATED_STRUCT(FVector2)

public:
	// 멤버 변수 (Member Variables)
	union
	{
		struct
		{
			UPROPERTY() float X;
			UPROPERTY() float Y;
		};
		float Data[2];
	};

	static const FVector2 ZeroVector;
	static const FVector2 OneVector;

public:
	// 생성자 (Constructors)
	constexpr FVector2() noexcept : X(0.0f), Y(0.0f) {}
	constexpr FVector2(float InX, float InY) noexcept : X(InX), Y(InY) {}
	explicit FVector2(const Float2& InFloat2) noexcept : X(InFloat2.x), Y(InFloat2.y) {}

	// 요소 접근 연산자 (Element Access Operators)
	constexpr float& operator[](int32 Index) noexcept
	{
		check(Index >= 0 && Index < 2);
		return Data[Index];
	}

	constexpr const float& operator[](int32 Index) const noexcept
	{
		check(Index >= 0 && Index < 2);
		return Data[Index];
	}

	// 비교 및 일반 사칙 연산자 (Comparison and Basic Math Operators)
	constexpr bool operator==(const FVector2& Other) const noexcept
	{
		return X == Other.X && Y == Other.Y;
	}

	constexpr bool operator!=(const FVector2& Other) const noexcept
	{
		return !(*this == Other);
	}

	constexpr FVector2 operator-() const noexcept
	{
		return { -X, -Y };
	}

	constexpr FVector2 operator+(const FVector2& Other) const noexcept
	{
		return { X + Other.X, Y + Other.Y };
	}

	constexpr FVector2 operator-(const FVector2& Other) const noexcept
	{
		return { X - Other.X, Y - Other.Y };
	}

	constexpr FVector2 operator*(float Scalar) const noexcept
	{
		return { X * Scalar, Y * Scalar };
	}

	constexpr FVector2 operator*(const FVector2& Other) const noexcept
	{
		return { X * Other.X, Y * Other.Y };
	}

	FVector2 operator/(float Scalar) const noexcept
	{
		check(std::fabs(Scalar) > KMath::Epsilon);
		return *this * (1.0f / Scalar);
	}

	constexpr float operator|(const FVector2& Other) const noexcept
	{
		return X * Other.X + Y * Other.Y;
	}

	constexpr float operator^(const FVector2& Other) const noexcept
	{
		return X * Other.Y - Y * Other.X;
	}

	// 복합 대입 연산자 (Compound Assignment Operators)
	constexpr FVector2& operator+=(const FVector2& Other) noexcept
	{
		X += Other.X;
		Y += Other.Y;
		return *this;
	}

	constexpr FVector2& operator-=(const FVector2& Other) noexcept
	{
		X -= Other.X;
		Y -= Other.Y;
		return *this;
	}

	constexpr FVector2& operator*=(float Scalar) noexcept
	{
		X *= Scalar;
		Y *= Scalar;
		return *this;
	}

	FVector2& operator/=(float Scalar) noexcept
	{
		check(std::fabs(Scalar) > KMath::Epsilon);
		return *this *= 1.0f / Scalar;
	}

	// 인스턴스 유틸리티 함수 (Instance Utility Functions)
	Float2 ToFloat2() const noexcept
	{
		return { X, Y };
	}

	bool Equals(const FVector2& Other, float Tolerance = KMath::Epsilon) const noexcept
	{
		return std::fabs(X - Other.X) <= Tolerance && std::fabs(Y - Other.Y) <= Tolerance;
	}

	constexpr bool IsZero() const noexcept
	{
		return X == 0.0f && Y == 0.0f;
	}

	bool IsNearlyZero(float Tolerance = KMath::Epsilon) const noexcept
	{
		return std::fabs(X) <= Tolerance && std::fabs(Y) <= Tolerance;
	}

	constexpr float SizeSquared() const noexcept
	{
		return X * X + Y * Y;
	}

	float Size() const noexcept;
	bool Normalize(float Tolerance = KMath::Epsilon) noexcept;
	FVector2 GetSafeNormal(float Tolerance = KMath::Epsilon) const noexcept;

	// 공용 벡터 계산기 (Static Vector Functions)
	static constexpr float DistSquared(const FVector2& A, const FVector2& B) noexcept
	{
		return (A - B).SizeSquared();
	}

	static float Dist(const FVector2& A, const FVector2& B) noexcept;
};

namespace std
{
	template <>
	struct hash<FVector2>
	{
		size_t operator()(const FVector2& Vector) const noexcept;
	};
} // namespace std
