#include "Core/Math/Vector2.h"

#include <cmath>

const FVector2 FVector2::ZeroVector{ 0.0f, 0.0f };
const FVector2 FVector2::OneVector{ 1.0f, 1.0f };

FVector2::FVector2(const Float2& InFloat2) noexcept
    : X(InFloat2.x), Y(InFloat2.y)
{
}

float& FVector2::operator[](int32 Index) noexcept
{
    check(Index >= 0 && Index < 2);
    return Data[Index];
}

const float& FVector2::operator[](int32 Index) const noexcept
{
    check(Index >= 0 && Index < 2);
    return Data[Index];
}

bool FVector2::operator==(const FVector2& Other) const noexcept
{
    return X == Other.X && Y == Other.Y;
}

bool FVector2::operator!=(const FVector2& Other) const noexcept
{
    return !(*this == Other);
}

FVector2 FVector2::operator-() const noexcept
{
    return { -X, -Y };
}

FVector2 FVector2::operator+(const FVector2& Other) const noexcept
{
    return { X + Other.X, Y + Other.Y };
}

FVector2 FVector2::operator-(const FVector2& Other) const noexcept
{
    return { X - Other.X, Y - Other.Y };
}

FVector2 FVector2::operator*(float Scalar) const noexcept
{
    return { X * Scalar, Y * Scalar };
}

FVector2 FVector2::operator*(const FVector2& Other) const noexcept
{
    return { X * Other.X, Y * Other.Y };
}

FVector2 FVector2::operator/(float Scalar) const noexcept
{
    check(std::fabs(Scalar) > KMath::Epsilon);
    return *this * (1.0f / Scalar);
}

float FVector2::operator|(const FVector2& Other) const noexcept
{
    return X * Other.X + Y * Other.Y;
}

float FVector2::operator^(const FVector2& Other) const noexcept
{
    return X * Other.Y - Y * Other.X;
}

FVector2& FVector2::operator+=(const FVector2& Other) noexcept
{
    X += Other.X;
    Y += Other.Y;
    return *this;
}

FVector2& FVector2::operator-=(const FVector2& Other) noexcept
{
    X -= Other.X;
    Y -= Other.Y;
    return *this;
}

FVector2& FVector2::operator*=(float Scalar) noexcept
{
    X *= Scalar;
    Y *= Scalar;
    return *this;
}

FVector2& FVector2::operator/=(float Scalar) noexcept
{
    check(std::fabs(Scalar) > KMath::Epsilon);
    return *this *= 1.0f / Scalar;
}

Float2 FVector2::ToFloat2() const noexcept
{
    return { X, Y };
}

bool FVector2::Equals(const FVector2& Other, float Tolerance) const noexcept
{
    return std::fabs(X - Other.X) <= Tolerance && std::fabs(Y - Other.Y) <= Tolerance;
}

bool FVector2::IsZero() const noexcept
{
    return X == 0.0f && Y == 0.0f;
}

bool FVector2::IsNearlyZero(float Tolerance) const noexcept
{
    return std::fabs(X) <= Tolerance && std::fabs(Y) <= Tolerance;
}

float FVector2::SizeSquared() const noexcept
{
    return X * X + Y * Y;
}

float FVector2::Size() const noexcept
{
    return std::sqrt(SizeSquared());
}

bool FVector2::Normalize(float Tolerance) noexcept
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

FVector2 FVector2::GetSafeNormal(float Tolerance) const noexcept
{
    FVector2 Result = *this;
    Result.Normalize(Tolerance);
    return Result;
}

float FVector2::DistSquared(const FVector2& A, const FVector2& B) noexcept
{
    return (A - B).SizeSquared();
}

float FVector2::Dist(const FVector2& A, const FVector2& B) noexcept
{
    return (A - B).Size();
}

size_t std::hash<FVector2>::operator()(const FVector2& Vector) const noexcept
{
    const size_t HashX = std::hash<float>{}(Vector.X);
    const size_t HashY = std::hash<float>{}(Vector.Y);
    return HashX ^ (HashY * 2654435761u + 0x9e3779b9u + (HashX << 6) + (HashX >> 2));
}
