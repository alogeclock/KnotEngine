#include "Core/Math/Quat.h"

#include "Core/Math/Matrix.h"
#include "Core/Math/Rotator.h"

#include <algorithm>
#include <cmath>

const FQuat FQuat::Identity;

FQuat::FQuat(const FVector& Axis, float AngleRad) noexcept
{
	const FVector NormalizedAxis = Axis.GetSafeNormal();
	if (NormalizedAxis.IsNearlyZero())
	{
		return;
	}

	const float HalfAngle = AngleRad * 0.5f;
	const float SinHalfAngle = std::sin(HalfAngle);
	X = NormalizedAxis.X * SinHalfAngle;
	Y = NormalizedAxis.Y * SinHalfAngle;
	Z = NormalizedAxis.Z * SinHalfAngle;
	W = std::cos(HalfAngle);
}

FQuat::FQuat(const FRotator& Rotator) noexcept
{
	*this = Rotator.Quaternion();
}

FQuat::FQuat(const FMatrix& Matrix) noexcept
{
	FVector Translation;
	FVector Scale;
	FMatrix Rotation;
	if (Matrix.Decompose(Translation, Rotation, Scale))
	{
		*this = FromRotationMatrix(Rotation);
	}
}

void FQuat::Normalize(float Tolerance) noexcept
{
	const float SquaredSize = SizeSquared();
	if (SquaredSize <= Tolerance)
	{
		*this = Identity;
		return;
	}
	*this *= 1.0f / std::sqrt(SquaredSize);
}

FQuat FQuat::GetNormalized(float Tolerance) const noexcept
{
	FQuat Result = *this;
	Result.Normalize(Tolerance);
	return Result;
}

FQuat FQuat::Inverse(float Tolerance) const noexcept
{
	const float SquaredSize = SizeSquared();
	if (SquaredSize <= Tolerance)
	{
		return Identity;
	}
	return FQuat(-X, -Y, -Z, W) / SquaredSize;
}

FVector FQuat::RotateVector(const FVector& Vector) const noexcept
{
	const FQuat Normalized = GetNormalized();
	const FVector QuatVector(Normalized.X, Normalized.Y, Normalized.Z);
	const FVector T = (QuatVector ^ Vector) * 2.0f;
	return Vector + T * Normalized.W + (QuatVector ^ T);
}

FVector FQuat::UnrotateVector(const FVector& Vector) const noexcept
{
	return Inverse().RotateVector(Vector);
}

FVector FQuat::GetForward() const noexcept
{
	return RotateVector(FVector::ForwardVector);
}

FVector FQuat::GetRight() const noexcept
{
	return RotateVector(FVector::RightVector);
}

FVector FQuat::GetUp() const noexcept
{
	return RotateVector(FVector::UpVector);
}

FRotator FQuat::Rotator() const noexcept
{
	const FMatrix RotationMatrix = ToMatrix();
	const float PitchRadians = std::asin(std::clamp(RotationMatrix.M[2][0], -1.0f, 1.0f));
	const float CosPitch = std::cos(PitchRadians);

	float YawRadians = 0.0f;
	float RollRadians = 0.0f;
	if (std::fabs(CosPitch) > KMath::Epsilon)
	{
		YawRadians = std::atan2(-RotationMatrix.M[1][0], RotationMatrix.M[0][0]);
		RollRadians = std::atan2(-RotationMatrix.M[2][1], RotationMatrix.M[2][2]);
	}
	else
	{
		YawRadians = std::atan2(RotationMatrix.M[0][1], RotationMatrix.M[1][1]);
	}

	FRotator Result(
	    KMath::ToDegree(PitchRadians),
	    KMath::ToDegree(YawRadians),
	    KMath::ToDegree(RollRadians));
	Result.Normalize();
	return Result;
}

FMatrix FQuat::ToMatrix() const noexcept
{
	const FQuat Q = GetNormalized();
	const float XX = Q.X * Q.X;
	const float YY = Q.Y * Q.Y;
	const float ZZ = Q.Z * Q.Z;
	const float XY = Q.X * Q.Y;
	const float XZ = Q.X * Q.Z;
	const float YZ = Q.Y * Q.Z;
	const float XW = Q.X * Q.W;
	const float YW = Q.Y * Q.W;
	const float ZW = Q.Z * Q.W;

	return FMatrix(
	    1.0f - 2.0f * (YY + ZZ), 2.0f * (XY + ZW), 2.0f * (XZ - YW), 0.0f,
	    2.0f * (XY - ZW), 1.0f - 2.0f * (XX + ZZ), 2.0f * (YZ + XW), 0.0f,
	    2.0f * (XZ + YW), 2.0f * (YZ - XW), 1.0f - 2.0f * (XX + YY), 0.0f,
	    0.0f, 0.0f, 0.0f, 1.0f);
}

FQuat FQuat::Slerp(const FQuat& A, const FQuat& B, float Alpha) noexcept
{
	const FQuat NormalizedA = A.GetNormalized();
	FQuat NormalizedB = B.GetNormalized();
	float Dot = NormalizedA | NormalizedB;
	if (Dot < 0.0f)
	{
		NormalizedB = -NormalizedB;
		Dot = -Dot;
	}

	if (Dot > 0.9995f)
	{
		return (NormalizedA + (NormalizedB - NormalizedA) * Alpha).GetNormalized();
	}

	const float Theta = std::acos(std::clamp(Dot, -1.0f, 1.0f));
	const float SinTheta = std::sin(Theta);
	return (NormalizedA * (std::sin((1.0f - Alpha) * Theta) / SinTheta) +
	        NormalizedB * (std::sin(Alpha * Theta) / SinTheta))
	    .GetNormalized();
}

FQuat FQuat::FromRotationMatrix(const FMatrix& Matrix) noexcept
{
	const float Trace = Matrix.M[0][0] + Matrix.M[1][1] + Matrix.M[2][2];
	FQuat Result;

	if (Trace > 0.0f)
	{
		const float S = std::sqrt(Trace + 1.0f) * 2.0f;
		Result.W = 0.25f * S;
		Result.X = (Matrix.M[1][2] - Matrix.M[2][1]) / S;
		Result.Y = (Matrix.M[2][0] - Matrix.M[0][2]) / S;
		Result.Z = (Matrix.M[0][1] - Matrix.M[1][0]) / S;
	}
	else if (Matrix.M[0][0] > Matrix.M[1][1] && Matrix.M[0][0] > Matrix.M[2][2])
	{
		const float S = std::sqrt(1.0f + Matrix.M[0][0] - Matrix.M[1][1] - Matrix.M[2][2]) * 2.0f;
		Result.W = (Matrix.M[1][2] - Matrix.M[2][1]) / S;
		Result.X = 0.25f * S;
		Result.Y = (Matrix.M[0][1] + Matrix.M[1][0]) / S;
		Result.Z = (Matrix.M[0][2] + Matrix.M[2][0]) / S;
	}
	else if (Matrix.M[1][1] > Matrix.M[2][2])
	{
		const float S = std::sqrt(1.0f + Matrix.M[1][1] - Matrix.M[0][0] - Matrix.M[2][2]) * 2.0f;
		Result.W = (Matrix.M[2][0] - Matrix.M[0][2]) / S;
		Result.X = (Matrix.M[0][1] + Matrix.M[1][0]) / S;
		Result.Y = 0.25f * S;
		Result.Z = (Matrix.M[1][2] + Matrix.M[2][1]) / S;
	}
	else
	{
		const float S = std::sqrt(1.0f + Matrix.M[2][2] - Matrix.M[0][0] - Matrix.M[1][1]) * 2.0f;
		Result.W = (Matrix.M[0][1] - Matrix.M[1][0]) / S;
		Result.X = (Matrix.M[0][2] + Matrix.M[2][0]) / S;
		Result.Y = (Matrix.M[1][2] + Matrix.M[2][1]) / S;
		Result.Z = 0.25f * S;
	}

	return Result.GetNormalized();
}
