#pragma once

#include <initializer_list>

#include "Core/CoreTypes.h"
#include "Core/Math/Vector4.h"

struct FConstRow;

struct FRow
{
	float* Values;

	float& operator[](const int32& Column) noexcept;
	const float& operator[](const int32& Column) const noexcept;
	FRow& operator=(const FRow& Row) noexcept;
	FRow& operator=(const FConstRow& Row) noexcept;
	FRow& operator=(const FVector4& Row) noexcept;
	FRow& operator=(const Float4& Row) noexcept;
	FRow& operator=(std::initializer_list<float> Row) noexcept;

	operator FVector4() const noexcept;
	operator float*() noexcept;
	operator const float*() const noexcept;

	FRow& operator+=(const FVector4& Row) noexcept;
	FRow& operator-=(const FVector4& Row) noexcept;
	FRow& operator*=(float Scalar) noexcept;
	FRow& operator/=(float Scalar) noexcept;

	FVector4 operator+(const FVector4& Row) const noexcept;
	FVector4 operator-(const FVector4& Row) const noexcept;
	FVector4 operator*(const FVector4& Row) const noexcept;
	FVector4 operator/(const FVector4& Row) const noexcept;
	FVector4 operator*(float Scalar) const noexcept;
	FVector4 operator/(float Scalar) const noexcept;
};

struct FConstRow
{
	const float* Values;

	const float& operator[](int32 Column) const noexcept;
	operator FVector4() const noexcept;
	operator const float*() const noexcept;

	FVector4 operator+(const FVector4& Row) const noexcept;
	FVector4 operator-(const FVector4& Row) const noexcept;
	FVector4 operator*(const FVector4& Row) const noexcept;
	FVector4 operator/(const FVector4& Row) const noexcept;
	FVector4 operator*(float Scalar) const noexcept;
	FVector4 operator/(float Scalar) const noexcept;
};

FVector4 operator*(float Scalar, const FRow& Row) noexcept;
FVector4 operator*(float Scalar, const FConstRow& Row) noexcept;
