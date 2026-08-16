#pragma once

#include "Core/CoreTypes.h"
#include "Core/Math/Math.h"

struct FVector2
{
public:
	// 멤버 변수 (Member Variables)
	union
	{
		struct
		{
			float X;
			float Y;
		};
		float Data[2];
	};

	static const FVector2 ZeroVector;
	static const FVector2 OneVector;

public:
	// 생성자 (Constructors)
	constexpr FVector2() noexcept
	    : X(0.0f), Y(0.0f) {}
	constexpr FVector2(float InX, float InY) noexcept
	    : X(InX), Y(InY) {}
	explicit FVector2(const Float2& InFloat2) noexcept;

	// 요소 접근 연산자 (Element Access Operators)
	float& operator[](int32 Index) noexcept;
	const float& operator[](int32 Index) const noexcept;

	// 비교 및 일반 사칙 연산자 (Comparison and Basic Math Operators)
	bool operator==(const FVector2& Other) const noexcept;
	bool operator!=(const FVector2& Other) const noexcept;
	FVector2 operator-() const noexcept;
	FVector2 operator+(const FVector2& Other) const noexcept;
	FVector2 operator-(const FVector2& Other) const noexcept;
	FVector2 operator*(float Scalar) const noexcept;
	FVector2 operator*(const FVector2& Other) const noexcept;
	FVector2 operator/(float Scalar) const noexcept;
	float operator|(const FVector2& Other) const noexcept;
	float operator^(const FVector2& Other) const noexcept;

	// 복합 대입 연산자 (Compound Assignment Operators)
	FVector2& operator+=(const FVector2& Other) noexcept;
	FVector2& operator-=(const FVector2& Other) noexcept;
	FVector2& operator*=(float Scalar) noexcept;
	FVector2& operator/=(float Scalar) noexcept;

	// 인스턴스 유틸리티 함수 (Instance Utility Functions)
	Float2 ToFloat2() const noexcept;
	bool Equals(const FVector2& Other, float Tolerance = KMath::Epsilon) const noexcept;
	bool IsZero() const noexcept;
	bool IsNearlyZero(float Tolerance = KMath::Epsilon) const noexcept;
	float SizeSquared() const noexcept;
	float Size() const noexcept;
	bool Normalize(float Tolerance = KMath::Epsilon) noexcept;
	FVector2 GetSafeNormal(float Tolerance = KMath::Epsilon) const noexcept;

	// 공용 벡터 계산기 (Static Vector Functions)
	static float DistSquared(const FVector2& A, const FVector2& B) noexcept;
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
