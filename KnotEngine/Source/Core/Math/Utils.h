#pragma once

namespace MathUtil
{
    static constexpr float Epsilon = 1.e-6f;

    static constexpr float Pi = 3.14159265358979323846f;
    static constexpr float InvPi = 0.31830988618379067154f;
    static constexpr float HalfPi = 1.57079632679489661923f;
    static constexpr float Tau = 6.28318530717958647692f;
    static constexpr float TO_RADIAN = Pi / 180;
    static constexpr float TO_DEG = 180 / Pi;

    static constexpr float ToRadian(float Degrees)
    {
        return Degrees * (Pi / 180.0f);
    }

    static constexpr float ToDegree(float Radians)
    {
        return Radians * (180.0f / Pi);
    }

    static constexpr float Abs(float Value)
    {
        return (Value < 0.0f) ? -Value : Value;
    }

    static constexpr bool IsNearlyZero(float Value, float Tolerance = Epsilon)
    {
        return Abs(Value) <= Tolerance;
    }

    static constexpr bool IsNearlyEqual(float A, float B, float Tolerance = Epsilon)
    {
        return Abs(A - B) <= Tolerance;
    }

    template <typename T> static inline T Clamp(const T Value, const T Min, const T Max)
    {
        return (Value < Min) ? Min : (Value > Max) ? Max : Value;
    }

    template <typename T> static inline T Max3(const T A, const T B, const T C)
    {
        return std::max(A, std::max(B, C));
    }

    template <typename T> static inline T Min3(const T A, const T B, const T C)
    {
        return std::min(A, std::min(B, C));
    }

	template <typename T, typename U> static inline T Lerp(const T& A, const T& B, const U& Alpha)
    {
        return A + (B - A) * Alpha;
    }

} // namespace
