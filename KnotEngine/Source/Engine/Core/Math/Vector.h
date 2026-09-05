#pragma once

#include "EngineAPI.h"

#include "Object/Reflection/ReflectionMacros.h"

#include <cmath>

#include "Core/CoreTypes.h"
#include "Core/Math/Math.h"

USTRUCT()
struct ENGINE_API FVector
{
	GENERATED_STRUCT(FVector)

public:
	// 멤버 변수 (Member Variables)
	union
	{
		struct
		{
			UPROPERTY() float X;
			UPROPERTY() float Y;
			UPROPERTY() float Z;
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
	explicit FVector(const Float3& InFloat3) noexcept : X(InFloat3.x), Y(InFloat3.y), Z(InFloat3.z) {}

	// 요소 접근 연산자 (Element Access Operators)
	constexpr float& operator[](int32 Index) noexcept
	{
		check(Index >= 0 && Index < 3);
		return Data[Index];
	}

	constexpr const float& operator[](int32 Index) const noexcept
	{
		check(Index >= 0 && Index < 3);
		return Data[Index];
	}

	// 비교 및 일반 사칙 연산자 (Comparison and Basic Math Operators)
	constexpr bool operator==(const FVector& Other) const noexcept
	{
		return X == Other.X && Y == Other.Y && Z == Other.Z;
	}

	constexpr bool operator!=(const FVector& Other) const noexcept
	{
		return !(*this == Other);
	}

	constexpr FVector operator-() const noexcept
	{
		return { -X, -Y, -Z };
	}

	constexpr FVector operator+(const FVector& Other) const noexcept
	{
		return { X + Other.X, Y + Other.Y, Z + Other.Z };
	}

	constexpr FVector operator-(const FVector& Other) const noexcept
	{
		return { X - Other.X, Y - Other.Y, Z - Other.Z };
	}

	constexpr FVector operator*(float Scalar) const noexcept
	{
		return { X * Scalar, Y * Scalar, Z * Scalar };
	}

	constexpr FVector operator*(const FVector& Other) const noexcept
	{
		return { X * Other.X, Y * Other.Y, Z * Other.Z };
	}

	FVector operator/(float Scalar) const noexcept
	{
		check(std::fabs(Scalar) > KMath::Epsilon);
		return *this * (1.0f / Scalar);
	}

	constexpr float operator|(const FVector& Other) const noexcept
	{
		return X * Other.X + Y * Other.Y + Z * Other.Z;
	}

	constexpr FVector operator^(const FVector& Other) const noexcept
	{
		return {
			Y * Other.Z - Z * Other.Y,
			Z * Other.X - X * Other.Z,
			X * Other.Y - Y * Other.X,
		};
	}

	// 복합 대입 연산자 (Compound Assignment Operators)
	constexpr FVector& operator+=(const FVector& Other) noexcept
	{
		X += Other.X;
		Y += Other.Y;
		Z += Other.Z;
		return *this;
	}

	constexpr FVector& operator-=(const FVector& Other) noexcept
	{
		X -= Other.X;
		Y -= Other.Y;
		Z -= Other.Z;
		return *this;
	}

	constexpr FVector& operator*=(float Scalar) noexcept
	{
		X *= Scalar;
		Y *= Scalar;
		Z *= Scalar;
		return *this;
	}

	FVector& operator/=(float Scalar) noexcept
	{
		check(std::fabs(Scalar) > KMath::Epsilon);
		return *this *= 1.0f / Scalar;
	}

	// 인스턴스 유틸리티 함수 (Instance Utility Functions)
	Float3 ToFloat3() const noexcept
	{
		return { X, Y, Z };
	}

	bool Equals(const FVector& Other, float Tolerance = KMath::Epsilon) const noexcept
	{
		return std::fabs(X - Other.X) <= Tolerance &&
		       std::fabs(Y - Other.Y) <= Tolerance &&
		       std::fabs(Z - Other.Z) <= Tolerance;
	}

	constexpr bool IsZero() const noexcept
	{
		return X == 0.0f && Y == 0.0f && Z == 0.0f;
	}

	bool IsNearlyZero(float Tolerance = KMath::Epsilon) const noexcept
	{
		return std::fabs(X) <= Tolerance && std::fabs(Y) <= Tolerance && std::fabs(Z) <= Tolerance;
	}

	constexpr float SizeSquared() const noexcept
	{
		return X * X + Y * Y + Z * Z;
	}

	float Size() const noexcept;
	bool Normalize(float Tolerance = KMath::Epsilon) noexcept;
	FVector GetSafeNormal(float Tolerance = KMath::Epsilon) const noexcept;

	// 공용 벡터 계산기 (Static Vector Functions)
	static constexpr float DistSquared(const FVector& A, const FVector& B) noexcept
	{
		return (A - B).SizeSquared();
	}

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
