#include "Core/Math/Vector4.h"

#include <cmath>

const FVector4 FVector4::ZeroVector{ 0.0f, 0.0f, 0.0f, 0.0f };

float& FVector4::operator[](int32 Index) noexcept
{
    check(Index >= 0 && Index < 4);
    return Data[Index];
}

const float& FVector4::operator[](int32 Index) const noexcept
{
    check(Index >= 0 && Index < 4);
    return Data[Index];
}

bool FVector4::operator==(const FVector4& Other) const noexcept
{
    return X == Other.X && Y == Other.Y && Z == Other.Z && W == Other.W;
}

bool FVector4::operator!=(const FVector4& Other) const noexcept
{
    return !(*this == Other);
}

FVector4 FVector4::operator-() const noexcept
{
    return { -X, -Y, -Z, -W };
}

FVector4 FVector4::operator+(const FVector4& Other) const noexcept
{
    return { X + Other.X, Y + Other.Y, Z + Other.Z, W + Other.W };
}

FVector4 FVector4::operator-(const FVector4& Other) const noexcept
{
    return { X - Other.X, Y - Other.Y, Z - Other.Z, W - Other.W };
}

FVector4 FVector4::operator*(float Scalar) const noexcept
{
    return { X * Scalar, Y * Scalar, Z * Scalar, W * Scalar };
}

FVector4 FVector4::operator*(const FVector4& Other) const noexcept
{
    return { X * Other.X, Y * Other.Y, Z * Other.Z, W * Other.W };
}

FVector4 FVector4::operator/(float Scalar) const noexcept
{
    check(std::fabs(Scalar) > KMath::Epsilon);
    return *this * (1.0f / Scalar);
}

float FVector4::operator|(const FVector4& Other) const noexcept
{
    return X * Other.X + Y * Other.Y + Z * Other.Z + W * Other.W;
}

FVector4& FVector4::operator+=(const FVector4& Other) noexcept
{
    X += Other.X;
    Y += Other.Y;
    Z += Other.Z;
    W += Other.W;
    return *this;
}

FVector4& FVector4::operator-=(const FVector4& Other) noexcept
{
    X -= Other.X;
    Y -= Other.Y;
    Z -= Other.Z;
    W -= Other.W;
    return *this;
}

FVector4& FVector4::operator*=(float Scalar) noexcept
{
    X *= Scalar;
    Y *= Scalar;
    Z *= Scalar;
    W *= Scalar;
    return *this;
}

FVector4& FVector4::operator/=(float Scalar) noexcept
{
    check(std::fabs(Scalar) > KMath::Epsilon);
    return *this *= 1.0f / Scalar;
}

bool FVector4::Equals(const FVector4& Other, float Tolerance) const noexcept
{
    return std::fabs(X - Other.X) <= Tolerance &&
           std::fabs(Y - Other.Y) <= Tolerance &&
           std::fabs(Z - Other.Z) <= Tolerance &&
           std::fabs(W - Other.W) <= Tolerance;
}

bool FVector4::IsZero() const noexcept
{
    return X == 0.0f && Y == 0.0f && Z == 0.0f && W == 0.0f;
}

bool FVector4::IsNearlyZero(float Tolerance) const noexcept
{
    return std::fabs(X) <= Tolerance && std::fabs(Y) <= Tolerance &&
           std::fabs(Z) <= Tolerance && std::fabs(W) <= Tolerance;
}

float FVector4::SizeSquared() const noexcept
{
    return X * X + Y * Y + Z * Z + W * W;
}

float FVector4::Size() const noexcept
{
    return std::sqrt(SizeSquared());
}

bool FVector4::Normalize(float Tolerance) noexcept
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

FVector4 FVector4::GetSafeNormal(float Tolerance) const noexcept
{
    FVector4 Result = *this;
    Result.Normalize(Tolerance);
    return Result;
}

bool FVector4::IsPoint(float Tolerance) const noexcept
{
    return std::fabs(W - 1.0f) <= Tolerance;
}

bool FVector4::IsVector(float Tolerance) const noexcept
{
    return std::fabs(W) <= Tolerance;
}

FVector FVector4::ToVector3(float Tolerance) const noexcept
{
    if (std::fabs(W) <= Tolerance)
    {
        return { X, Y, Z };
    }
    return { X / W, Y / W, Z / W };
}
