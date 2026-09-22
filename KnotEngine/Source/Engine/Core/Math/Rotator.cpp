#include "Core/Math/Rotator.h"

#include "Core/Math/Quat.h"

#include <cmath>

const FRotator FRotator::ZeroRotator;

FRotator::FRotator(const FQuat& Quat) noexcept
{
	*this = Quat.Rotator();
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

// 누적된 각도를 360도 회전량과 정규화된 나머지로 분리한다.
void FRotator::GetWindingAndRemainder(FRotator& Winding, FRotator& Remainder) const noexcept
{
	Remainder = GetNormalized();
	Winding = *this - Remainder;
}

// 두 동등한 표현 중 이 회전값과 축별 차이의 합이 작은 표현을 선택한다.
void FRotator::SetClosest(FRotator& Other) const noexcept
{
	const FRotator Alternative = FRotator(180.0f - Other.Pitch, Other.Yaw + 180.0f, Other.Roll + 180.0f);
	const FRotator FirstDelta = Other - *this;
	const FRotator SecondDelta = Alternative - *this;
	const float FirstDistance = std::fabs(FirstDelta.Pitch) + std::fabs(FirstDelta.Yaw) + std::fabs(FirstDelta.Roll);
	const float SecondDistance = std::fabs(SecondDelta.Pitch) + std::fabs(SecondDelta.Yaw) + std::fabs(SecondDelta.Roll);
	if (SecondDistance < FirstDistance)
	{
		Other = Alternative;
	}
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
	const float HalfPitch = KMath::ToRadian(Pitch) * 0.5f;
	const float HalfYaw = KMath::ToRadian(Yaw) * 0.5f;
	const float HalfRoll = KMath::ToRadian(Roll) * 0.5f;

	const float SinPitch = std::sin(HalfPitch);
	const float CosPitch = std::cos(HalfPitch);
	const float SinYaw = std::sin(HalfYaw);
	const float CosYaw = std::cos(HalfYaw);
	const float SinRoll = std::sin(HalfRoll);
	const float CosRoll = std::cos(HalfRoll);

	return FQuat(
	    SinRoll * CosPitch * CosYaw + CosRoll * SinPitch * SinYaw,
	    CosRoll * SinPitch * CosYaw - SinRoll * CosPitch * SinYaw,
	    CosRoll * CosPitch * SinYaw + SinRoll * SinPitch * CosYaw,
	    CosRoll * CosPitch * CosYaw - SinRoll * SinPitch * SinYaw);
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
