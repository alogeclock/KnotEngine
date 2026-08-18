#pragma once

#include <cmath>

#include "Core/Math/Vector.h"

struct FMatrix;

struct alignas(16) FVector4
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
	constexpr float& operator[](int32 Index) noexcept
	{
		check(Index >= 0 && Index < 4);
		return Data[Index];
	}

	constexpr const float& operator[](int32 Index) const noexcept
	{
		check(Index >= 0 && Index < 4);
		return Data[Index];
	}

	// 비교 및 일반 사칙 연산자 (Comparison and Basic Math Operators)
	constexpr bool operator==(const FVector4& Other) const noexcept
	{
		return X == Other.X && Y == Other.Y && Z == Other.Z && W == Other.W;
	}

	constexpr bool operator!=(const FVector4& Other) const noexcept
	{
		return !(*this == Other);
	}

	constexpr FVector4 operator-() const noexcept
	{
		return { -X, -Y, -Z, -W };
	}

	constexpr FVector4 operator+(const FVector4& Other) const noexcept
	{
		return { X + Other.X, Y + Other.Y, Z + Other.Z, W + Other.W };
	}

	constexpr FVector4 operator-(const FVector4& Other) const noexcept
	{
		return { X - Other.X, Y - Other.Y, Z - Other.Z, W - Other.W };
	}

	constexpr FVector4 operator*(float Scalar) const noexcept
	{
		return { X * Scalar, Y * Scalar, Z * Scalar, W * Scalar };
	}

	constexpr FVector4 operator*(const FVector4& Other) const noexcept
	{
		return { X * Other.X, Y * Other.Y, Z * Other.Z, W * Other.W };
	}

	FVector4 operator/(float Scalar) const noexcept
	{
		check(std::fabs(Scalar) > KMath::Epsilon);
		return *this * (1.0f / Scalar);
	}

	constexpr FVector4 operator*(const FMatrix& Matrix) const noexcept;

	constexpr float operator|(const FVector4& Other) const noexcept
	{
		return X * Other.X + Y * Other.Y + Z * Other.Z + W * Other.W;
	}

	// 복합 대입 연산자 (Compound Assignment Operators)
	constexpr FVector4& operator+=(const FVector4& Other) noexcept
	{
		X += Other.X;
		Y += Other.Y;
		Z += Other.Z;
		W += Other.W;
		return *this;
	}

	constexpr FVector4& operator-=(const FVector4& Other) noexcept
	{
		X -= Other.X;
		Y -= Other.Y;
		Z -= Other.Z;
		W -= Other.W;
		return *this;
	}

	constexpr FVector4& operator*=(float Scalar) noexcept
	{
		X *= Scalar;
		Y *= Scalar;
		Z *= Scalar;
		W *= Scalar;
		return *this;
	}

	FVector4& operator/=(float Scalar) noexcept
	{
		check(std::fabs(Scalar) > KMath::Epsilon);
		return *this *= 1.0f / Scalar;
	}

	// 인스턴스 유틸리티 함수 (Instance Utility Functions)
	bool Equals(const FVector4& Other, float Tolerance = KMath::Epsilon) const noexcept
	{
		return std::fabs(X - Other.X) <= Tolerance &&
		       std::fabs(Y - Other.Y) <= Tolerance &&
		       std::fabs(Z - Other.Z) <= Tolerance &&
		       std::fabs(W - Other.W) <= Tolerance;
	}

	constexpr bool IsZero() const noexcept
	{
		return X == 0.0f && Y == 0.0f && Z == 0.0f && W == 0.0f;
	}

	bool IsNearlyZero(float Tolerance = KMath::Epsilon) const noexcept
	{
		return std::fabs(X) <= Tolerance && std::fabs(Y) <= Tolerance &&
		       std::fabs(Z) <= Tolerance && std::fabs(W) <= Tolerance;
	}

	constexpr float SizeSquared() const noexcept
	{
		return X * X + Y * Y + Z * Z + W * W;
	}

	float Size() const noexcept;
	bool Normalize(float Tolerance = KMath::Epsilon) noexcept;
	FVector4 GetSafeNormal(float Tolerance = KMath::Epsilon) const noexcept;

	// 동차 좌표 유틸리티 함수 (Homogeneous Coordinate Utility Functions)
	bool IsPoint(float Tolerance = KMath::Epsilon) const noexcept
	{
		return std::fabs(W - 1.0f) <= Tolerance;
	}

	bool IsVector(float Tolerance = KMath::Epsilon) const noexcept
	{
		return std::fabs(W) <= Tolerance;
	}

	FVector ToVector3(float Tolerance = KMath::Epsilon) const noexcept
	{
		if (std::fabs(W) <= Tolerance)
		{
			return { X, Y, Z };
		}
		return { X / W, Y / W, Z / W };
	}

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

static_assert(sizeof(FVector4) == 16);
static_assert(alignof(FVector4) == 16);
