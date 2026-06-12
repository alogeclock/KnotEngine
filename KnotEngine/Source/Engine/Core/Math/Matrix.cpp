#include "Core/Math/Matrix.h"

#include <cmath>

namespace
{
XMVector LoadRow(const float Row[4]) noexcept
{
	return DirectX::XMVectorSet(Row[0], Row[1], Row[2], Row[3]);
}

FVector4 StoreVector4(XMVector Vector) noexcept
{
	Float4 Temp;
	DirectX::XMStoreFloat4(&Temp, Vector);
	return FVector4(Temp.x, Temp.y, Temp.z, Temp.w);
}

FMatrix MakeMatrixFromRows(XMVector Row0, XMVector Row1, XMVector Row2, XMVector Row3) noexcept
{
	const FVector4 R0 = StoreVector4(Row0);
	const FVector4 R1 = StoreVector4(Row1);
	const FVector4 R2 = StoreVector4(Row2);
	const FVector4 R3 = StoreVector4(Row3);
	return FMatrix(R0, R1, R2, R3);
}
} // namespace

FMatrix FMatrix::operator+(const FMatrix& Other) const noexcept
{
	return MakeMatrixFromRows(
		DirectX::XMVectorAdd(LoadRow(M[0]), LoadRow(Other.M[0])),
		DirectX::XMVectorAdd(LoadRow(M[1]), LoadRow(Other.M[1])),
		DirectX::XMVectorAdd(LoadRow(M[2]), LoadRow(Other.M[2])),
		DirectX::XMVectorAdd(LoadRow(M[3]), LoadRow(Other.M[3])));
}

FMatrix FMatrix::operator-(const FMatrix& Other) const noexcept
{
	return MakeMatrixFromRows(
		DirectX::XMVectorSubtract(LoadRow(M[0]), LoadRow(Other.M[0])),
		DirectX::XMVectorSubtract(LoadRow(M[1]), LoadRow(Other.M[1])),
		DirectX::XMVectorSubtract(LoadRow(M[2]), LoadRow(Other.M[2])),
		DirectX::XMVectorSubtract(LoadRow(M[3]), LoadRow(Other.M[3])));
}

FMatrix FMatrix::operator*(float Scalar) const noexcept
{
	const XMVector Scale = DirectX::XMVectorReplicate(Scalar);
	return MakeMatrixFromRows(
		DirectX::XMVectorMultiply(LoadRow(M[0]), Scale),
		DirectX::XMVectorMultiply(LoadRow(M[1]), Scale),
		DirectX::XMVectorMultiply(LoadRow(M[2]), Scale),
		DirectX::XMVectorMultiply(LoadRow(M[3]), Scale));
}

FMatrix FMatrix::operator/(float Scalar) const noexcept
{
	assert(Scalar != 0.f);
	return *this * (1.0f / Scalar);
}

FMatrix& FMatrix::operator+=(const FMatrix& Other) noexcept
{
	*this = *this + Other;
	return *this;
}

FMatrix& FMatrix::operator-=(const FMatrix& Other) noexcept
{
	*this = *this - Other;
	return *this;
}

FMatrix& FMatrix::operator*=(float Scalar) noexcept
{
	*this = *this * Scalar;
	return *this;
}

FMatrix& FMatrix::operator/=(float Scalar) noexcept
{
	assert(Scalar != 0.f);
	*this = *this / Scalar;
	return *this;
}

FMatrix FMatrix::operator*(const FMatrix& Other) const noexcept
{
	return FMatrix(DirectX::XMMatrixMultiply(ToXMMatrix(), Other.ToXMMatrix()));
}

FMatrix& FMatrix::operator*=(const FMatrix& Other) noexcept
{
	*this = *this * Other;
	return *this;
}

bool FMatrix::Equals(const FMatrix& Other, float Tolerance) const noexcept
{
	const XMVector ToleranceVector = DirectX::XMVectorReplicate(Tolerance);
	for (int32 Row = 0; Row < 4; ++Row)
	{
		if (!DirectX::XMVector4NearEqual(LoadRow(M[Row]), LoadRow(Other.M[Row]), ToleranceVector))
		{
			return false;
		}
	}
	return true;
}

FMatrix FMatrix::GetTransposed() const noexcept
{
	return FMatrix(DirectX::XMMatrixTranspose(ToXMMatrix()));
}

FVector FMatrix::TransformVector(const FVector& V) const noexcept
{
	return FVector(DirectX::XMVector3TransformNormal(V.ToXMVector(), ToXMMatrix()));
}

FVector FMatrix::TransformPosition(const FVector& V) const noexcept
{
	return FVector(DirectX::XMVector3TransformCoord(V.ToXMVector(), ToXMMatrix()));
}

FVector FMatrix::TransformPositionWithW(const FVector& V) const noexcept
{
	return FVector(DirectX::XMVector4Transform(V.ToXMVector(), ToXMMatrix()));
}

FVector4 FMatrix::TransformVector4(const FVector4& V, const FMatrix& Matrix) const noexcept
{
	const XMVector Transformed = DirectX::XMVector4Transform(V.ToXMVector(), Matrix.ToXMMatrix());
	return StoreVector4(Transformed);
}

FVector FMatrix::GetScaleVector() const noexcept
{
	const XMVector XAxis = DirectX::XMVectorSet(M[0][0], M[0][1], M[0][2], 0.0f);
	const XMVector YAxis = DirectX::XMVectorSet(M[1][0], M[1][1], M[1][2], 0.0f);
	const XMVector ZAxis = DirectX::XMVectorSet(M[2][0], M[2][1], M[2][2], 0.0f);

	return FVector(
		DirectX::XMVectorGetX(DirectX::XMVector3Length(XAxis)),
		DirectX::XMVectorGetX(DirectX::XMVector3Length(YAxis)),
		DirectX::XMVectorGetX(DirectX::XMVector3Length(ZAxis)));
}

float FMatrix::Determinant() const noexcept
{
	const XMMatrix XM = ToXMMatrix();
	const XMVector Det = DirectX::XMMatrixDeterminant(XM);
	return DirectX::XMVectorGetX(Det);
}

FMatrix FMatrix::GetInverse(float Tolerance) const noexcept
{
	const XMMatrix XM = ToXMMatrix();

	XMVector Det;
	const XMMatrix Inv = DirectX::XMMatrixInverse(&Det, XM);

	const float DeterminantValue = DirectX::XMVectorGetX(Det);
	if (std::fabs(DeterminantValue) <= Tolerance)
	{
#ifndef NDEBUG
		assert("FMatrix::GetInverse() failed: matrix is singular or invalid.");
#endif
		return Identity;
	}

	return FMatrix(Inv);
}

bool FMatrix::Inverse(float Tolerance) noexcept
{
	const XMMatrix XM = ToXMMatrix();

	XMVector Det;
	const XMMatrix Inv = DirectX::XMMatrixInverse(&Det, XM);

	const float DeterminantValue = DirectX::XMVectorGetX(Det);
	if (std::fabs(DeterminantValue) <= Tolerance)
	{
		*this = Identity;
		return false;
	}

	*this = FMatrix(Inv);
	return true;
}

bool FMatrix::IsInvertible(float Tolerance) const noexcept
{
	return std::fabs(Determinant()) > Tolerance;
}

bool FMatrix::Decompose(FVector& OutTranslation, FMatrix& OutRotation, FVector& OutScale, float Tolerance) const noexcept
{
	OutTranslation = GetOrigin();

	const FVector XAxis = GetScaledAxis(EAxis::X);
	const FVector YAxis = GetScaledAxis(EAxis::Y);
	const FVector ZAxis = GetScaledAxis(EAxis::Z);

	OutScale = FVector(XAxis.Size(), YAxis.Size(), ZAxis.Size());

	if (OutScale.X <= Tolerance || OutScale.Y <= Tolerance || OutScale.Z <= Tolerance)
	{
		OutRotation = Identity;
		return false;
	}

	const FVector UnitX = XAxis / OutScale.X;
	const FVector UnitY = YAxis / OutScale.Y;
	const FVector UnitZ = ZAxis / OutScale.Z;

	OutRotation = Identity;
	OutRotation.SetAxes(UnitX, UnitY, UnitZ, FVector::ZeroVector);

	return true;
}

FMatrix FMatrix::MakeTranslation(const FVector& Translation) noexcept
{
	return FMatrix(FVector::XAxisVector, FVector::YAxisVector, FVector::ZAxisVector, FVector4(Translation, 1.0f));
}

FMatrix FMatrix::MakeScale(const FVector& Scale) noexcept
{
	return FMatrix(Scale.X, 0.f, 0.f, 0.f, 0.f, Scale.Y, 0.f, 0.f, 0.f, 0.f, Scale.Z, 0.f, 0.f, 0.f, 0.f, 1.f);
}

FMatrix FMatrix::MakeRotationX(float AngleRad) noexcept
{
	const float CosAngle = std::cos(AngleRad);
	const float SinAngle = std::sin(AngleRad);

	return FMatrix(1.f, 0.f, 0.f, 0.f, 0.f, CosAngle, SinAngle, 0.f, 0.f, -SinAngle, CosAngle, 0.f, 0.f, 0.f, 0.f, 1.f);
}

FMatrix FMatrix::MakeRotationY(float AngleRad) noexcept
{
	const float CosAngle = std::cos(AngleRad);
	const float SinAngle = std::sin(AngleRad);

	return FMatrix(CosAngle, 0.f, -SinAngle, 0.f, 0.f, 1.f, 0.f, 0.f, SinAngle, 0.f, CosAngle, 0.f, 0.f, 0.f, 0.f, 1.f);
}

FMatrix FMatrix::MakeRotationZ(float AngleRad) noexcept
{
	const float CosAngle = std::cos(AngleRad);
	const float SinAngle = std::sin(AngleRad);

	return FMatrix(CosAngle, SinAngle, 0.f, 0.f, -SinAngle, CosAngle, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f);
}

FMatrix FMatrix::MakeScale(float Scale) noexcept
{
	return MakeScale(FVector(Scale, Scale, Scale));
}

FMatrix FMatrix::MakeScaleMatrix(float Scale) noexcept
{
	return MakeScale(Scale);
}

FMatrix FMatrix::MakeRotationAxis(const FVector& Axis, float AngleRad) noexcept
{
	const FVector N = Axis.GetSafeNormal();
	if (N.IsNearlyZero())
	{
		return Identity;
	}

	const float C = std::cos(AngleRad);
	const float S = std::sin(AngleRad);
	const float T = 1.0f - C;

	return FMatrix(
		T * N.X * N.X + C,       T * N.X * N.Y + S * N.Z, T * N.X * N.Z - S * N.Y, 0.0f,
		T * N.X * N.Y - S * N.Z, T * N.Y * N.Y + C,       T * N.Y * N.Z + S * N.X, 0.0f,
		T * N.X * N.Z + S * N.Y, T * N.Y * N.Z - S * N.X, T * N.Z * N.Z + C,       0.0f,
		0.0f,                    0.0f,                    0.0f,                    1.0f);
}

FMatrix FMatrix::MakeRotationEuler(const FVector& EulerDegrees) noexcept
{
	const float DegToRad = 3.14159265358979323846f / 180.0f;
	const FMatrix Rx = MakeRotationX(EulerDegrees.X * DegToRad);
	const FMatrix Ry = MakeRotationY(EulerDegrees.Y * DegToRad);
	const FMatrix Rz = MakeRotationZ(EulerDegrees.Z * DegToRad);
	return Rx * Ry * Rz;
}

FVector FMatrix::GetEuler() const noexcept
{
	const FVector Forward = GetForward();
	const FVector Right = GetRight();

	const float Pitch = std::atan2(Forward.Z, std::sqrt(Forward.X * Forward.X + Forward.Y * Forward.Y));
	const float Yaw = std::atan2(Forward.Y, Forward.X);
	const float Roll = std::atan2(-Right.Z, M[2][2]);

	return FVector(KMath::ToDegree(Pitch), KMath::ToDegree(Yaw), KMath::ToDegree(Roll));
}

FMatrix FMatrix::MakeFromX(const FVector& XAxis) noexcept
{
	const FVector X = XAxis.GetSafeNormal();
	if (X.IsNearlyZero())
	{
		return Identity;
	}

	const FVector UpCandidate = (std::fabs(X.Z) < 0.999f) ? FVector::UpVector : FVector::RightVector;
	const FVector Y = FVector::Cross(UpCandidate, X).GetSafeNormal();
	const FVector Z = FVector::Cross(X, Y).GetSafeNormal();

	return FMatrix(FVector4(X, 0.f), FVector4(Y, 0.f), FVector4(Z, 0.0f), FVector4::ZeroPoint());
}

FMatrix FMatrix::MakeFromY(const FVector& YAxis) noexcept
{
	const FVector Y = YAxis.GetSafeNormal();
	if (Y.IsNearlyZero())
	{
		return Identity;
	}

	const FVector UpCandidate = (std::fabs(Y.Z) < 0.999f) ? FVector::UpVector : FVector::ForwardVector;
	const FVector X = FVector::Cross(Y, UpCandidate).GetSafeNormal();
	const FVector Z = FVector::Cross(X, Y).GetSafeNormal();

	return FMatrix(X.X, X.Y, X.Z, 0.f, Y.X, Y.Y, Y.Z, 0.f, Z.X, Z.Y, Z.Z, 0.f, 0.f, 0.f, 0.f, 1.f);
}

FMatrix FMatrix::MakeFromZ(const FVector& ZAxis) noexcept
{
	const FVector Z = ZAxis.GetSafeNormal();
	if (Z.IsNearlyZero())
	{
		return Identity;
	}

	const FVector ForwardCandidate = (std::fabs(Z.X) < 0.999f) ? FVector::ForwardVector : FVector::RightVector;
	const FVector Y = FVector::Cross(Z, ForwardCandidate).GetSafeNormal();
	const FVector X = FVector::Cross(Y, Z).GetSafeNormal();

	return FMatrix(X.X, X.Y, X.Z, 0.f, Y.X, Y.Y, Y.Z, 0.f, Z.X, Z.Y, Z.Z, 0.f, 0.f, 0.f, 0.f, 1.f);
}

FMatrix FMatrix::MakeLookAt(const FVector& Eye, const FVector& Target, const FVector& Up) noexcept
{
	const FVector Forward = (Target - Eye).GetSafeNormal();
	if (Forward.IsNearlyZero())
	{
		return Identity;
	}

	const FVector Right = FVector::Cross(Up, Forward).GetSafeNormal();
	if (Right.IsNearlyZero())
	{
		return Identity;
	}

	const FVector NewUp = FVector::Cross(Forward, Right).GetSafeNormal();

	return FMatrix(Forward.X, Forward.Y, Forward.Z, 0.f, Right.X, Right.Y, Right.Z, 0.f,
				   NewUp.X, NewUp.Y, NewUp.Z, 0.f, Eye.X, Eye.Y, Eye.Z, 1.f);
}

FMatrix FMatrix::MakePerspectiveFovLH(float FovYRad, float AspectRatio, float NearZ, float FarZ) noexcept
{
	assert(AspectRatio != 0.f);
	assert(FarZ != NearZ);

	const float YScale = 1.0f / std::tan(FovYRad * 0.5f);
	const float XScale = YScale / AspectRatio;

	return FMatrix(
		0.f,    0.f,    FarZ / (FarZ - NearZ),          1.f,
		XScale, 0.f,    0.f,                            0.f,
		0.f,    YScale, 0.f,                            0.f,
		0.f,    0.f,    -NearZ * FarZ / (FarZ - NearZ), 0.f);
}

FMatrix FMatrix::MakeOrthographicLH(float ViewWidth, float ViewHeight, float NearZ, float FarZ) noexcept
{
	assert(ViewWidth != 0.f);
	assert(ViewHeight != 0.f);
	assert(FarZ != NearZ);

	return FMatrix(
		0.f,             0.f,              1.f / (FarZ - NearZ),    0.f,
		2.f / ViewWidth, 0.f,              0.f,                     0.f,
		0.f,             2.f / ViewHeight, 0.f,                     0.f,
		0.f,             0.f,              -NearZ / (FarZ - NearZ), 1.f);
}

FMatrix FMatrix::MakeViewLookAtLH(const FVector& Eye, const FVector& Target, const FVector& Up) noexcept
{
	const FVector Forward = (Target - Eye).GetSafeNormal();
	if (Forward.IsNearlyZero())
	{
		return Identity;
	}

	const FVector Right = FVector::Cross(Up, Forward).GetSafeNormal();
	if (Right.IsNearlyZero())
	{
		return Identity;
	}

	const FVector NewUp = FVector::Cross(Forward, Right).GetSafeNormal();

	return FMatrix(
		Forward.X,                   Right.X,                   NewUp.X,                   0.f,
		Forward.Y,                   Right.Y,                   NewUp.Y,                   0.f,
		Forward.Z,                   Right.Z,                   NewUp.Z,                   0.f,
		-FVector::Dot(Eye, Forward), -FVector::Dot(Eye, Right), -FVector::Dot(Eye, NewUp), 1.f);
}

FMatrix FMatrix::MakeBillboard(const FVector& Position, const FVector& CameraPosition, const FVector& Up) noexcept
{
	const FVector Forward = (CameraPosition - Position).GetSafeNormal();
	if (Forward.IsNearlyZero())
	{
		return MakeTranslation(Position);
	}

	const FVector Right = FVector::Cross(Up, Forward).GetSafeNormal();
	if (Right.IsNearlyZero())
	{
		return MakeTranslation(Position);
	}

	const FVector NewUp = FVector::Cross(Forward, Right).GetSafeNormal();

	return FMatrix(
		Forward.X, Forward.Y, Forward.Z, 0.f,
		Right.X, Right.Y, Right.Z, 0.f,
		NewUp.X, NewUp.Y, NewUp.Z, 0.f,
		Position.X, Position.Y, Position.Z, 1.f);
}

FMatrix FMatrix::MakeWorld(const FVector& Translation, const FMatrix& RotationMatrix, const FVector& Scale) noexcept
{
	FMatrix Result = RotationMatrix;

	Result.M[0][0] *= Scale.X;
	Result.M[0][1] *= Scale.X;
	Result.M[0][2] *= Scale.X;

	Result.M[1][0] *= Scale.Y;
	Result.M[1][1] *= Scale.Y;
	Result.M[1][2] *= Scale.Y;

	Result.M[2][0] *= Scale.Z;
	Result.M[2][1] *= Scale.Z;
	Result.M[2][2] *= Scale.Z;

	Result.M[3][0] = Translation.X;
	Result.M[3][1] = Translation.Y;
	Result.M[3][2] = Translation.Z;
	Result.M[3][3] = 1.f;

	return Result;
}

FMatrix FMatrix::MakeTRS(const FVector& Translation, const FMatrix& RotationMatrix, const FVector& Scale) noexcept
{
	return MakeWorld(Translation, RotationMatrix, Scale);
}

FVector4 FVector4::operator*(const FMatrix& Mat) const noexcept
{
	const XMVector Result = DirectX::XMVector4Transform(ToXMVector(), Mat.ToXMMatrix());
	return StoreVector4(Result);
}
