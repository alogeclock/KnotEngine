#include "Core/Math/Matrix.h"

#include <cmath>

const FMatrix FMatrix::Identity;

FMatrix FMatrix::GetInverse(float Tolerance) const noexcept
{
	FMatrix Result;
	if (!TryInverse(*this, Result, Tolerance))
	{
		return Identity;
	}
	return Result;
}

bool FMatrix::Decompose(FVector& OutTranslation, FMatrix& OutRotation, FVector& OutScale, float Tolerance) const noexcept
{
	OutTranslation = { M[3][0], M[3][1], M[3][2] };

	const FVector AxisX = GetScaledAxis(EAxis::X);
	const FVector AxisY = GetScaledAxis(EAxis::Y);
	const FVector AxisZ = GetScaledAxis(EAxis::Z);
	OutScale = { AxisX.Size(), AxisY.Size(), AxisZ.Size() };

	if (OutScale.X <= Tolerance || OutScale.Y <= Tolerance || OutScale.Z <= Tolerance)
	{
		OutRotation = Identity;
		return false;
	}

	OutRotation = FMatrix(
		FVector4(AxisX / OutScale.X, 0.0f),
		FVector4(AxisY / OutScale.Y, 0.0f),
		FVector4(AxisZ / OutScale.Z, 0.0f),
		FVector4::Point());
	return true;
}

FMatrix FMatrix::MakeTranslation(const FVector& Translation) noexcept
{
	FMatrix Result;
	Result.M[3][0] = Translation.X;
	Result.M[3][1] = Translation.Y;
	Result.M[3][2] = Translation.Z;
	return Result;
}

FMatrix FMatrix::MakeScale(const FVector& Scale) noexcept
{
	return FMatrix(
		Scale.X, 0.0f, 0.0f, 0.0f,
		0.0f, Scale.Y, 0.0f, 0.0f,
		0.0f, 0.0f, Scale.Z, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f);
}

FMatrix FMatrix::MakeRotationX(float AngleRad) noexcept
{
	const float CosAngle = std::cos(AngleRad);
	const float SinAngle = std::sin(AngleRad);
	return FMatrix(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, CosAngle, SinAngle, 0.0f,
		0.0f, -SinAngle, CosAngle, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f);
}

FMatrix FMatrix::MakeRotationY(float AngleRad) noexcept
{
	const float CosAngle = std::cos(AngleRad);
	const float SinAngle = std::sin(AngleRad);
	return FMatrix(
		CosAngle, 0.0f, -SinAngle, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		SinAngle, 0.0f, CosAngle, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f);
}

FMatrix FMatrix::MakeRotationZ(float AngleRad) noexcept
{
	const float CosAngle = std::cos(AngleRad);
	const float SinAngle = std::sin(AngleRad);
	return FMatrix(
		CosAngle, SinAngle, 0.0f, 0.0f,
		-SinAngle, CosAngle, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f);
}

FMatrix FMatrix::MakePerspectiveFov(float FovYRad, float AspectRatio, float NearZ, float FarZ) noexcept
{
	check(AspectRatio > 0.0f);
	check(NearZ > 0.0f && FarZ > NearZ);

	const float YScale = 1.0f / std::tan(FovYRad * 0.5f);
	const float XScale = YScale / AspectRatio;
	return FMatrix(
		XScale, 0.0f, 0.0f, 0.0f,
		0.0f, YScale, 0.0f, 0.0f,
		0.0f, 0.0f, FarZ / (FarZ - NearZ), 1.0f,
		0.0f, 0.0f, -NearZ * FarZ / (FarZ - NearZ), 0.0f);
}

FMatrix FMatrix::MakeOrthographic(
	float ViewWidth,
	float ViewHeight,
	float NearZ,
	float FarZ) noexcept
{
	check(ViewWidth > 0.0f && ViewHeight > 0.0f);
	check(FarZ > NearZ);

	return FMatrix(
		2.0f / ViewWidth, 0.0f, 0.0f, 0.0f,
		0.0f, 2.0f / ViewHeight, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f / (FarZ - NearZ), 0.0f,
		0.0f, 0.0f, -NearZ / (FarZ - NearZ), 1.0f);
}

FMatrix FMatrix::MakeLookAt(const FVector& Eye, const FVector& Target, const FVector& Up) noexcept
{
	const FVector Forward = (Target - Eye).GetSafeNormal();
	const FVector Right = (Up ^ Forward).GetSafeNormal();
	const FVector CorrectedUp = (Forward ^ Right).GetSafeNormal();

	if (Forward.IsNearlyZero() || Right.IsNearlyZero() || CorrectedUp.IsNearlyZero())
	{
		return Identity;
	}

	return FMatrix(
		Right.X, CorrectedUp.X, Forward.X, 0.0f,
		Right.Y, CorrectedUp.Y, Forward.Y, 0.0f,
		Right.Z, CorrectedUp.Z, Forward.Z, 0.0f,
		-(Eye | Right), -(Eye | CorrectedUp), -(Eye | Forward), 1.0f);
}

FMatrix FMatrix::MakeWorld(const FVector& Translation, const FMatrix& Rotation, const FVector& Scale) noexcept
{
	FMatrix Result = Rotation;
	for (int32 Column = 0; Column < 3; ++Column)
	{
		Result.M[0][Column] *= Scale.X;
		Result.M[1][Column] *= Scale.Y;
		Result.M[2][Column] *= Scale.Z;
	}

	Result.M[3][0] = Translation.X;
	Result.M[3][1] = Translation.Y;
	Result.M[3][2] = Translation.Z;
	Result.M[3][3] = 1.0f;
	return Result;
}

bool FMatrix::TryInverse(const FMatrix& Matrix, FMatrix& OutInverse, float Tolerance) noexcept
{
	float Augmented[4][8]{};
	for (int32 Row = 0; Row < 4; ++Row)
	{
		for (int32 Column = 0; Column < 4; ++Column)
		{
			Augmented[Row][Column] = Matrix.M[Row][Column];
			Augmented[Row][Column + 4] = Row == Column ? 1.0f : 0.0f;
		}
	}

	for (int32 Column = 0; Column < 4; ++Column)
	{
		int32 PivotRow = Column;
		float PivotMagnitude = std::fabs(Augmented[PivotRow][Column]);
		for (int32 Row = Column + 1; Row < 4; ++Row)
		{
			const float CandidateMagnitude = std::fabs(Augmented[Row][Column]);
			if (CandidateMagnitude > PivotMagnitude)
			{
				PivotRow = Row;
				PivotMagnitude = CandidateMagnitude;
			}
		}

		if (PivotMagnitude <= Tolerance)
		{
			return false;
		}

		if (PivotRow != Column)
		{
			for (int32 Index = 0; Index < 8; ++Index)
			{
				const float Temp = Augmented[Column][Index];
				Augmented[Column][Index] = Augmented[PivotRow][Index];
				Augmented[PivotRow][Index] = Temp;
			}
		}

		const float InversePivot = 1.0f / Augmented[Column][Column];
		for (int32 Index = 0; Index < 8; ++Index)
		{
			Augmented[Column][Index] *= InversePivot;
		}

		for (int32 Row = 0; Row < 4; ++Row)
		{
			if (Row == Column)
			{
				continue;
			}

			const float Factor = Augmented[Row][Column];
			for (int32 Index = 0; Index < 8; ++Index)
			{
				Augmented[Row][Index] -= Factor * Augmented[Column][Index];
			}
		}
	}

	for (int32 Row = 0; Row < 4; ++Row)
	{
		for (int32 Column = 0; Column < 4; ++Column)
		{
			OutInverse.M[Row][Column] = Augmented[Row][Column + 4];
		}
	}
	return true;
}
