#include "Core/Math/Vector.h"

#include <cmath>

const FVector FVector::ZeroVector{ 0.0f, 0.0f, 0.0f };
const FVector FVector::OneVector{ 1.0f, 1.0f, 1.0f };
const FVector FVector::ForwardVector{ 1.0f, 0.0f, 0.0f };
const FVector FVector::RightVector{ 0.0f, 1.0f, 0.0f };
const FVector FVector::UpVector{ 0.0f, 0.0f, 1.0f };

FVector::FVector(const Float3& InFloat3) noexcept
	: X(InFloat3.x), Y(InFloat3.y), Z(InFloat3.z)
{
}

float& FVector::operator[](int32 Index) noexcept
{
	check(Index >= 0 && Index < 3);
	return Data[Index];
}

const float& FVector::operator[](int32 Index) const noexcept
{
	check(Index >= 0 && Index < 3);
	return Data[Index];
}

bool FVector::operator==(const FVector& Other) const noexcept
{
	return X == Other.X && Y == Other.Y && Z == Other.Z;
}

bool FVector::operator!=(const FVector& Other) const noexcept
{
	return !(*this == Other);
}

FVector FVector::operator-() const noexcept
{
	return { -X, -Y, -Z };
}

FVector FVector::operator+(const FVector& Other) const noexcept
{
	return { X + Other.X, Y + Other.Y, Z + Other.Z };
}

FVector FVector::operator-(const FVector& Other) const noexcept
{
	return { X - Other.X, Y - Other.Y, Z - Other.Z };
}

FVector FVector::operator*(float Scalar) const noexcept
{
	return { X * Scalar, Y * Scalar, Z * Scalar };
}

FVector FVector::operator*(const FVector& Other) const noexcept
{
	return { X * Other.X, Y * Other.Y, Z * Other.Z };
}

FVector FVector::operator/(float Scalar) const noexcept
{
	check(std::fabs(Scalar) > KMath::Epsilon);
	return *this * (1.0f / Scalar);
}

float FVector::operator|(const FVector& Other) const noexcept
{
	return X * Other.X + Y * Other.Y + Z * Other.Z;
}

FVector FVector::operator^(const FVector& Other) const noexcept
{
	return {
		Y * Other.Z - Z * Other.Y,
		Z * Other.X - X * Other.Z,
		X * Other.Y - Y * Other.X,
	};
}

FVector& FVector::operator+=(const FVector& Other) noexcept
{
	X += Other.X;
	Y += Other.Y;
	Z += Other.Z;
	return *this;
}

FVector& FVector::operator-=(const FVector& Other) noexcept
{
	X -= Other.X;
	Y -= Other.Y;
	Z -= Other.Z;
	return *this;
}

FVector& FVector::operator*=(float Scalar) noexcept
{
	X *= Scalar;
	Y *= Scalar;
	Z *= Scalar;
	return *this;
}

FVector& FVector::operator/=(float Scalar) noexcept
{
	check(std::fabs(Scalar) > KMath::Epsilon);
	return *this *= 1.0f / Scalar;
}

Float3 FVector::ToFloat3() const noexcept
{
	return { X, Y, Z };
}

bool FVector::Equals(const FVector& Other, float Tolerance) const noexcept
{
	return std::fabs(X - Other.X) <= Tolerance &&
		   std::fabs(Y - Other.Y) <= Tolerance &&
		   std::fabs(Z - Other.Z) <= Tolerance;
}

bool FVector::IsZero() const noexcept
{
	return X == 0.0f && Y == 0.0f && Z == 0.0f;
}

bool FVector::IsNearlyZero(float Tolerance) const noexcept
{
	return std::fabs(X) <= Tolerance && std::fabs(Y) <= Tolerance && std::fabs(Z) <= Tolerance;
}

float FVector::SizeSquared() const noexcept
{
	return X * X + Y * Y + Z * Z;
}

float FVector::Size() const noexcept
{
	return std::sqrt(SizeSquared());
}

bool FVector::Normalize(float Tolerance) noexcept
{
	const float SquaredSize = SizeSquared();
	if (SquaredSize <= Tolerance)
	{
		*this = ZeroVector;
		return false;
	}

	*this *= 1.0f / std::sqrt(SquaredSize);
	return true;
}

FVector FVector::GetSafeNormal(float Tolerance) const noexcept
{
	FVector Result = *this;
	Result.Normalize(Tolerance);
	return Result;
}

float FVector::DistSquared(const FVector& A, const FVector& B) noexcept
{
	return (A - B).SizeSquared();
}

float FVector::Dist(const FVector& A, const FVector& B) noexcept
{
	return (A - B).Size();
}

size_t std::hash<FVector>::operator()(const FVector& Vector) const noexcept
{
	size_t Hash = std::hash<float>{}(Vector.X);
	Hash ^= std::hash<float>{}(Vector.Y) * 2654435761u + 0x9e3779b9u + (Hash << 6) + (Hash >> 2);
	Hash ^= std::hash<float>{}(Vector.Z) * 2654435761u + 0x9e3779b9u + (Hash << 6) + (Hash >> 2);
	return Hash;
}
