#pragma once

#include <initializer_list>

#include "Core/CoreTypes.h"
#include "Core/Math/Vector4.h"

struct FConstRow;

struct FRow
{
	// 멤버 변수 (Member Variables)
	float* Values;

	// 요소 접근 연산자 (Element Access Operators)
	float& operator[](const int32& Column) noexcept;
	const float& operator[](const int32& Column) const noexcept;

	// 대입 및 변환 연산자 (Assignment and Conversion Operators)
	FRow& operator=(const FRow& Row) noexcept;
	FRow& operator=(const FConstRow& Row) noexcept;
	FRow& operator=(const FVector4& Row) noexcept;
	FRow& operator=(const Float4& Row) noexcept;
	FRow& operator=(std::initializer_list<float> Row) noexcept;

	operator FVector4() const noexcept;
	operator float*() noexcept;
	operator const float*() const noexcept;

	// 복합 대입 연산자 (Compound Assignment Operators)
	FRow& operator+=(const FVector4& Row) noexcept;
	FRow& operator-=(const FVector4& Row) noexcept;
	FRow& operator*=(float Scalar) noexcept;
	FRow& operator/=(float Scalar) noexcept;

	// 일반 사칙 연산자 (Basic Math Operators)
	FVector4 operator+(const FVector4& Row) const noexcept;
	FVector4 operator-(const FVector4& Row) const noexcept;
	FVector4 operator*(const FVector4& Row) const noexcept;
	FVector4 operator/(const FVector4& Row) const noexcept;
	FVector4 operator*(float Scalar) const noexcept;
	FVector4 operator/(float Scalar) const noexcept;
};

struct FConstRow
{
	// 멤버 변수 (Member Variables)
	const float* Values;

	// 요소 접근 및 변환 연산자 (Element Access and Conversion Operators)
	const float& operator[](int32 Column) const noexcept;
	operator FVector4() const noexcept;
	operator const float*() const noexcept;

	// 일반 사칙 연산자 (Basic Math Operators)
	FVector4 operator+(const FVector4& Row) const noexcept;
	FVector4 operator-(const FVector4& Row) const noexcept;
	FVector4 operator*(const FVector4& Row) const noexcept;
	FVector4 operator/(const FVector4& Row) const noexcept;
	FVector4 operator*(float Scalar) const noexcept;
	FVector4 operator/(float Scalar) const noexcept;
};

// 전역 산술 연산자 (Global Arithmetic Operators)
FVector4 operator*(float Scalar, const FRow& Row) noexcept;
FVector4 operator*(float Scalar, const FConstRow& Row) noexcept;
