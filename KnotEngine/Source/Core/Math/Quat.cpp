#include "Math/Quat.h"

#include "Math/Matrix.h"
#include "Math/Rotator.h"

#include <algorithm>
#include <cmath>

FQuat::FQuat(const FRotator& InRotator) noexcept
{
	*this = InRotator.Quaternion();
}

FQuat::FQuat(const FMatrix& InMatrix) noexcept
	: X(0.f), Y(0.f), Z(0.f), W(1.f)
{
	XMVector OutScale, OutRotation, OutTranslation;
	if (DirectX::XMMatrixDecompose(&OutScale, &OutRotation, &OutTranslation, InMatrix.ToXMMatrix()))
	{
		*this = FQuat(OutRotation);
		Normalize();
		return;
	}

	const FMatrix RotationSource = InMatrix.GetMatrixWithoutTranslation();
	const FVector XAxis = RotationSource.GetScaledAxis(EAxis::X);
	const FVector YAxis = RotationSource.GetScaledAxis(EAxis::Y);
	const FVector ZAxis = RotationSource.GetScaledAxis(EAxis::Z);

	FVector OrthoX, OrthoY, OrthoZ;
	if (BuildOrthonormalBasisFromXY(XAxis, YAxis, OrthoX, OrthoY, OrthoZ)
		|| BuildOrthonormalBasisFromXZ(XAxis, ZAxis, OrthoX, OrthoY, OrthoZ)
		|| BuildOrthonormalBasisFromYZ(YAxis, ZAxis, OrthoX, OrthoY, OrthoZ))
	{
		FMatrix OrthonormalMatrix = FMatrix::Identity;
		OrthonormalMatrix.SetAxes(OrthoX, OrthoY, OrthoZ);
		*this = FQuat(DirectX::XMQuaternionRotationMatrix(OrthonormalMatrix.ToXMMatrix()));
		Normalize();
		return;
	}

	*this = Identity;
}

FQuat FQuat::MakeFromEuler(const FVector& InEulerDegrees) noexcept
{
	return FQuat(FRotator::MakeFromEuler(InEulerDegrees));
}

FVector FQuat::Euler() const noexcept
{
	return Rotator().Euler();
}

FRotator FQuat::Rotator() const noexcept
{
	const FMatrix RotationMatrix = ToMatrix();
	const float ClampedPitchSin  = std::clamp(RotationMatrix.M[2][0], -1.0f, 1.0f);
	const float PitchRadians     = std::asin(ClampedPitchSin);
	const float CosPitch         = std::cos(PitchRadians);

	float YawRadians  = 0.0f;
	float RollRadians = 0.0f;

	if (std::fabs(CosPitch) > KMath::Epsilon)
	{
		YawRadians  = std::atan2(-RotationMatrix.M[1][0], RotationMatrix.M[0][0]);
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
	return FMatrix(DirectX::XMMatrixRotationQuaternion(GetNormalized().ToXMVector()));
}

FRotator::FRotator(const FQuat& InQuat) noexcept
{
	*this = InQuat.Rotator();
}

FQuat FRotator::Quaternion() const noexcept
{
	const FMatrix RotationMatrix =
		FMatrix::MakeRotationZ(KMath::ToRadian(Yaw))
		* FMatrix::MakeRotationY(KMath::ToRadian(Pitch))
		* FMatrix::MakeRotationX(KMath::ToRadian(Roll));

	return FQuat(DirectX::XMQuaternionRotationMatrix(RotationMatrix.ToXMMatrix())).GetNormalized();
}

FVector FRotator::RotateVector(const FVector& InVector) const noexcept
{
	return Quaternion().RotateVector(InVector);
}

FVector FRotator::UnrotateVector(const FVector& InVector) const noexcept
{
	return Quaternion().UnrotateVector(InVector);
}

FRotator FRotator::GetInverse() const noexcept
{
	return Quaternion().Inverse().Rotator();
}
