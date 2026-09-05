#pragma once

#include "EngineAPI.h"

#include "Object/Reflection/ReflectionMacros.h"

#include <cmath>

#include "Core/Math/Math.h"
#include "Core/Math/Vector.h"

struct FMatrix;
struct FRotator;

USTRUCT()
struct ENGINE_API FQuat
{
	GENERATED_STRUCT(FQuat)

public:
	// 멤버 변수 (Member Variables)
	UPROPERTY() float X = 0.0f;
	UPROPERTY() float Y = 0.0f;
	UPROPERTY() float Z = 0.0f;
	UPROPERTY() float W = 1.0f;

	static const FQuat Identity;

public:
	// 생성자 (Constructors)
	constexpr FQuat() noexcept = default;
	constexpr FQuat(float InX, float InY, float InZ, float InW) noexcept : X(InX), Y(InY), Z(InZ), W(InW) {}
	FQuat(const FVector& Axis, float AngleRad) noexcept;
	explicit FQuat(const FRotator& Rotator) noexcept;
	explicit FQuat(const FMatrix& Matrix) noexcept;

	// 비교 및 일반 사칙 연산자 (Comparison and Basic Math Operators)
	constexpr bool operator==(const FQuat& Other) const noexcept
	{
		return X == Other.X && Y == Other.Y && Z == Other.Z && W == Other.W;
	}

	constexpr bool operator!=(const FQuat& Other) const noexcept
	{
		return !(*this == Other);
	}

	constexpr FQuat operator-() const noexcept
	{
		return { -X, -Y, -Z, -W };
	}

	constexpr FQuat operator+(const FQuat& Other) const noexcept
	{
		return { X + Other.X, Y + Other.Y, Z + Other.Z, W + Other.W };
	}

	constexpr FQuat operator-(const FQuat& Other) const noexcept
	{
		return { X - Other.X, Y - Other.Y, Z - Other.Z, W - Other.W };
	}

	constexpr FQuat operator*(float Scalar) const noexcept
	{
		return { X * Scalar, Y * Scalar, Z * Scalar, W * Scalar };
	}

	FQuat operator/(float Scalar) const noexcept
	{
		check(std::fabs(Scalar) > KMath::Epsilon);
		return *this * (1.0f / Scalar);
	}

	constexpr FQuat operator*(const FQuat& Other) const noexcept
	{
		return {
			W * Other.X + X * Other.W + Y * Other.Z - Z * Other.Y,
			W * Other.Y - X * Other.Z + Y * Other.W + Z * Other.X,
			W * Other.Z + X * Other.Y - Y * Other.X + Z * Other.W,
			W * Other.W - X * Other.X - Y * Other.Y - Z * Other.Z,
		};
	}

	FVector operator*(const FVector& Vector) const noexcept
	{
		return RotateVector(Vector);
	}

	constexpr float operator|(const FQuat& Other) const noexcept
	{
		return X * Other.X + Y * Other.Y + Z * Other.Z + W * Other.W;
	}

	// 복합 대입 연산자 (Compound Assignment Operators)
	constexpr FQuat& operator+=(const FQuat& Other) noexcept
	{
		X += Other.X;
		Y += Other.Y;
		Z += Other.Z;
		W += Other.W;
		return *this;
	}

	constexpr FQuat& operator-=(const FQuat& Other) noexcept
	{
		X -= Other.X;
		Y -= Other.Y;
		Z -= Other.Z;
		W -= Other.W;
		return *this;
	}

	constexpr FQuat& operator*=(float Scalar) noexcept
	{
		X *= Scalar;
		Y *= Scalar;
		Z *= Scalar;
		W *= Scalar;
		return *this;
	}

	FQuat& operator/=(float Scalar) noexcept
	{
		check(std::fabs(Scalar) > KMath::Epsilon);
		return *this *= 1.0f / Scalar;
	}

	constexpr FQuat& operator*=(const FQuat& Other) noexcept
	{
		*this = *this * Other;
		return *this;
	}

	// 인스턴스 유틸리티 함수 (Instance Utility Functions)
	bool Equals(const FQuat& Other, float Tolerance = KMath::Epsilon) const noexcept
	{
		const bool SameSign = std::fabs(X - Other.X) <= Tolerance &&
		                      std::fabs(Y - Other.Y) <= Tolerance &&
		                      std::fabs(Z - Other.Z) <= Tolerance &&
		                      std::fabs(W - Other.W) <= Tolerance;
		const bool OppositeSign = std::fabs(X + Other.X) <= Tolerance &&
		                          std::fabs(Y + Other.Y) <= Tolerance &&
		                          std::fabs(Z + Other.Z) <= Tolerance &&
		                          std::fabs(W + Other.W) <= Tolerance;
		return SameSign || OppositeSign;
	}

	constexpr float SizeSquared() const noexcept
	{
		return X * X + Y * Y + Z * Z + W * W;
	}

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
constexpr FQuat operator*(float Scalar, const FQuat& Quat) noexcept
{
	return Quat * Scalar;
}
