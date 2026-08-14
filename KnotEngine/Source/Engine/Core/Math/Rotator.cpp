#include "Core/Math/Rotator.h"

#include "Core/Math/Matrix.h"
#include "Core/Math/Quat.h"

#include <cmath>

const FRotator FRotator::ZeroRotator;

FRotator::FRotator(const FQuat& Quat) noexcept
{
    *this = Quat.Rotator();
}

bool FRotator::operator==(const FRotator& Other) const noexcept
{
    return Pitch == Other.Pitch && Yaw == Other.Yaw && Roll == Other.Roll;
}

bool FRotator::operator!=(const FRotator& Other) const noexcept
{
    return !(*this == Other);
}

FRotator FRotator::operator-() const noexcept
{
    return { -Pitch, -Yaw, -Roll };
}

FRotator FRotator::operator+(const FRotator& Other) const noexcept
{
    return { Pitch + Other.Pitch, Yaw + Other.Yaw, Roll + Other.Roll };
}

FRotator FRotator::operator-(const FRotator& Other) const noexcept
{
    return { Pitch - Other.Pitch, Yaw - Other.Yaw, Roll - Other.Roll };
}

FRotator FRotator::operator*(float Scalar) const noexcept
{
    return { Pitch * Scalar, Yaw * Scalar, Roll * Scalar };
}

FRotator FRotator::operator/(float Scalar) const noexcept
{
    check(std::fabs(Scalar) > KMath::Epsilon);
    return *this * (1.0f / Scalar);
}

FRotator& FRotator::operator+=(const FRotator& Other) noexcept
{
    Pitch += Other.Pitch;
    Yaw += Other.Yaw;
    Roll += Other.Roll;
    return *this;
}

FRotator& FRotator::operator-=(const FRotator& Other) noexcept
{
    Pitch -= Other.Pitch;
    Yaw -= Other.Yaw;
    Roll -= Other.Roll;
    return *this;
}

FRotator& FRotator::operator*=(float Scalar) noexcept
{
    Pitch *= Scalar;
    Yaw *= Scalar;
    Roll *= Scalar;
    return *this;
}

FRotator& FRotator::operator/=(float Scalar) noexcept
{
    check(std::fabs(Scalar) > KMath::Epsilon);
    return *this *= 1.0f / Scalar;
}

FVector FRotator::Euler() const noexcept
{
    return { Roll, Pitch, Yaw };
}

FVector FRotator::Vector() const noexcept
{
    const float PitchRadians = KMath::ToRadian(Pitch);
    const float YawRadians = KMath::ToRadian(Yaw);
    const float CosPitch = std::cos(PitchRadians);
    return FVector(
        CosPitch * std::cos(YawRadians),
        CosPitch * std::sin(YawRadians),
        std::sin(PitchRadians));
}

void FRotator::Normalize() noexcept
{
    Pitch = NormalizeAxis(Pitch);
    Yaw = NormalizeAxis(Yaw);
    Roll = NormalizeAxis(Roll);
}

FRotator FRotator::GetNormalized() const noexcept
{
    FRotator Result = *this;
    Result.Normalize();
    return Result;
}

bool FRotator::IsZero() const noexcept
{
    return NormalizeAxis(Pitch) == 0.0f &&
           NormalizeAxis(Yaw) == 0.0f &&
           NormalizeAxis(Roll) == 0.0f;
}

bool FRotator::IsNearlyZero(float Tolerance) const noexcept
{
    return std::fabs(NormalizeAxis(Pitch)) <= Tolerance &&
           std::fabs(NormalizeAxis(Yaw)) <= Tolerance &&
           std::fabs(NormalizeAxis(Roll)) <= Tolerance;
}

bool FRotator::Equals(const FRotator& Other, float Tolerance) const noexcept
{
    return std::fabs(NormalizeAxis(Pitch - Other.Pitch)) <= Tolerance &&
           std::fabs(NormalizeAxis(Yaw - Other.Yaw)) <= Tolerance &&
           std::fabs(NormalizeAxis(Roll - Other.Roll)) <= Tolerance;
}

FVector FRotator::RotateVector(const FVector& Vector) const noexcept
{
    return Quaternion().RotateVector(Vector);
}

FVector FRotator::UnrotateVector(const FVector& Vector) const noexcept
{
    return Quaternion().UnrotateVector(Vector);
}

FRotator FRotator::GetInverse() const noexcept
{
    return Quaternion().Inverse().Rotator();
}

FQuat FRotator::Quaternion() const noexcept
{
    const FMatrix RotationMatrix =
        FMatrix::MakeRotationZ(KMath::ToRadian(Yaw)) *
        FMatrix::MakeRotationY(KMath::ToRadian(Pitch)) *
        FMatrix::MakeRotationX(KMath::ToRadian(Roll));
    return FQuat(RotationMatrix).GetNormalized();
}

float FRotator::NormalizeAxis(float AngleDegrees) noexcept
{
    float Result = std::fmod(AngleDegrees, 360.0f);
    if (Result < 0.0f)
    {
        Result += 360.0f;
    }
    if (Result > 180.0f)
    {
        Result -= 360.0f;
    }
    return Result;
}

FRotator FRotator::MakeFromEuler(const FVector& EulerDegrees) noexcept
{
    return { EulerDegrees.Y, EulerDegrees.Z, EulerDegrees.X };
}

FRotator operator*(float Scalar, const FRotator& Rotator) noexcept
{
    return Rotator * Scalar;
}
