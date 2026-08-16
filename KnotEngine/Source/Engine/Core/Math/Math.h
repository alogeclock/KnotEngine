#pragma once

#include <algorithm>

namespace KMath
{
	// 수학 상수 (Math Constants)
	inline constexpr float Epsilon = 1.e-6f;

	inline constexpr float Pi = 3.14159265358979323846f;
	inline constexpr float InvPi = 0.31830988618379067154f;
	inline constexpr float HalfPi = 1.57079632679489661923f;
	inline constexpr float Tau = 6.28318530717958647692f;

	inline constexpr float TO_RADIAN = Pi / 180.0f;
	inline constexpr float TO_DEGREE = 180.0f / Pi;

	// 각도 변환 함수 (Angle Conversion Functions)
	constexpr float ToRadian(float Degrees) noexcept
	{
		return Degrees * TO_RADIAN;
	}

	constexpr float ToDegree(float Radians) noexcept
	{
		return Radians * TO_DEGREE;
	}

	// 부동 소수점 유틸리티 함수 (Floating-Point Utility Functions)
	constexpr float Abs(float Value) noexcept
	{
		return Value < 0.0f ? -Value : Value;
	}

	constexpr bool IsNearlyZero(float Value, float Tolerance = Epsilon) noexcept
	{
		return Abs(Value) <= Tolerance;
	}

	constexpr bool IsNearlyEqual(float A, float B, float Tolerance = Epsilon) noexcept
	{
		return Abs(A - B) <= Tolerance;
	}

	// 범용 수학 함수 (Generic Math Functions)
	template <typename T>
	constexpr T Clamp(const T& Value, const T& Min, const T& Max)
	{
		return Value < Min ? Min : Max < Value ? Max
		                                       : Value;
	}

	template <typename T>
	constexpr T Max3(const T& A, const T& B, const T& C)
	{
		return std::max(A, std::max(B, C));
	}

	template <typename T>
	constexpr T Min3(const T& A, const T& B, const T& C)
	{
		return std::min(A, std::min(B, C));
	}

	template <typename T, typename U>
	constexpr T Lerp(const T& A, const T& B, const U& Alpha)
	{
		return A + (B - A) * Alpha;
	}
} // namespace KMath
