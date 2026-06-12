#pragma once

#include <cassert>
#include <cmath>
#include <algorithm>

#include "Core/CoreTypes.h"
#include "Core/Math/Utils.h"
#include "Core/Math/Vector.h"

struct FRotator;
struct FMatrix;

struct FQuat
{
public:
	float X = 0.0f;
	float Y = 0.0f;
	float Z = 0.0f;
	float W = 0.0f;

	static const FQuat Identity;

	// ──────────── Constructor ────────────
public:
	constexpr FQuat() noexcept
		: X(0.f), Y(0.f), Z(0.f), W(1.f) {}

	constexpr FQuat(float InX, float InY, float InZ, float InW) noexcept
		: X(InX), Y(InY), Z(InZ), W(InW) {}

	explicit FQuat(FXMVector InVector) noexcept
		: X(0.f), Y(0.f), Z(0.f), W(1.f)
	{
		DirectX::XMFLOAT4 Temp;
		DirectX::XMStoreFloat4(&Temp, InVector);
		X = Temp.x; Y = Temp.y; Z = Temp.z; W = Temp.w;
	}

	FQuat(const FVector& Axis, float AngleRad) noexcept
		: X(0.f), Y(0.f), Z(0.f), W(1.f)
	{
		const FVector NormalizedAxis = Axis.GetSafeNormal();
		if (!NormalizedAxis.IsNearlyZero())
		{
			*this = FQuat(DirectX::XMQuaternionRotationAxis(NormalizedAxis.ToXMVector(), AngleRad));
			Normalize();
		}
	}

	// Defined after #include "Rotator.h" / "Matrix.h"
	explicit FQuat(const FRotator& InRotator) noexcept;
	explicit FQuat(const FMatrix& InMatrix) noexcept;

	FQuat(const FQuat&) noexcept = default;
	FQuat(FQuat&&) noexcept = default;

	// ──────────── Operators ────────────
public:
	FQuat& operator=(const FQuat&) noexcept = default;
	FQuat& operator=(FQuat&&) noexcept = default;

	bool operator==(const FQuat& Other) const noexcept
	{
		return X == Other.X && Y == Other.Y && Z == Other.Z && W == Other.W;
	}

	bool operator!=(const FQuat& Other) const noexcept { return !(*this == Other); }

	FQuat operator-() const noexcept { return FQuat(-X, -Y, -Z, -W); }

	FQuat operator+(const FQuat& Other) const noexcept
	{
		return FQuat(X + Other.X, Y + Other.Y, Z + Other.Z, W + Other.W);
	}

	FQuat operator-(const FQuat& Other) const noexcept
	{
		return FQuat(X - Other.X, Y - Other.Y, Z - Other.Z, W - Other.W);
	}

	FQuat operator*(float Scale) const noexcept
	{
		return FQuat(X * Scale, Y * Scale, Z * Scale, W * Scale);
	}

	FQuat operator/(float Scale) const noexcept
	{
		assert(std::fabs(Scale) > KMath::Epsilon);
		return FQuat(X / Scale, Y / Scale, Z / Scale, W / Scale);
	}

	FQuat operator*(const FQuat& Other) const noexcept
	{
		return FQuat(DirectX::XMQuaternionMultiply(ToXMVector(), Other.ToXMVector()));
	}

	FVector operator*(const FVector& InVector) const noexcept
	{
		return RotateVector(InVector);
	}

	FQuat& operator+=(const FQuat& Other) noexcept
	{
		X += Other.X; Y += Other.Y; Z += Other.Z; W += Other.W;
		return *this;
	}

	FQuat& operator-=(const FQuat& Other) noexcept
	{
		X -= Other.X; Y -= Other.Y; Z -= Other.Z; W -= Other.W;
		return *this;
	}

	FQuat& operator*=(float Scale) noexcept
	{
		X *= Scale; Y *= Scale; Z *= Scale; W *= Scale;
		return *this;
	}

	FQuat& operator/=(float Scale) noexcept
	{
		assert(std::fabs(Scale) > KMath::Epsilon);
		X /= Scale; Y /= Scale; Z /= Scale; W /= Scale;
		return *this;
	}

	FQuat& operator*=(const FQuat& Other) noexcept
	{
		*this = *this * Other;
		return *this;
	}

	float operator|(const FQuat& Other) const noexcept
	{
		return DotProduct(*this, Other);
	}

	// ──────────── Methods ────────────
public:
	XMVector ToXMVector() const noexcept
	{
		return DirectX::XMVectorSet(X, Y, Z, W);
	}

	bool Equals(const FQuat& Other, float Tolerance = KMath::Epsilon) const noexcept
	{
		const bool bSameSign =
			std::fabs(X - Other.X) <= Tolerance && std::fabs(Y - Other.Y) <= Tolerance &&
			std::fabs(Z - Other.Z) <= Tolerance && std::fabs(W - Other.W) <= Tolerance;

		const bool bNegatedSign =
			std::fabs(X + Other.X) <= Tolerance && std::fabs(Y + Other.Y) <= Tolerance &&
			std::fabs(Z + Other.Z) <= Tolerance && std::fabs(W + Other.W) <= Tolerance;

		return bSameSign || bNegatedSign;
	}

	bool IsIdentity(float Tolerance = KMath::Epsilon) const noexcept
	{
		return Equals(Identity, Tolerance);
	}

	bool ContainsNaN() const noexcept
	{
		return !std::isfinite(X) || !std::isfinite(Y) || !std::isfinite(Z) || !std::isfinite(W);
	}

	float SizeSquared() const noexcept
	{
		return DirectX::XMVectorGetX(DirectX::XMVector4LengthSq(ToXMVector()));
	}

	float Size() const noexcept
	{
		return DirectX::XMVectorGetX(DirectX::XMVector4Length(ToXMVector()));
	}

	bool IsNormalized(float Tolerance = KMath::Epsilon) const noexcept
	{
		return std::fabs(SizeSquared() - 1.0f) <= Tolerance;
	}

	void Normalize(float Tolerance = KMath::Epsilon) noexcept
	{
		const XMVector QuatVector = ToXMVector();
		const float SquaredSize = DirectX::XMVectorGetX(DirectX::XMVector4LengthSq(QuatVector));
		if (SquaredSize > Tolerance)
		{
			*this = FQuat(DirectX::XMQuaternionNormalize(QuatVector));
			return;
		}
		*this = Identity;
	}

	FQuat GetNormalized(float Tolerance = KMath::Epsilon) const noexcept
	{
		FQuat Result = *this;
		Result.Normalize(Tolerance);
		return Result;
	}

	FQuat Conjugate() const noexcept
	{
		return FQuat(DirectX::XMQuaternionConjugate(ToXMVector()));
	}

	FQuat Inverse() const noexcept
	{
		const float SquaredSize = SizeSquared();
		if (SquaredSize <= KMath::Epsilon)
		{
			return Identity;
		}
		return Conjugate() / SquaredSize;
	}

	FVector RotateVector(const FVector& InVector) const noexcept
	{
		return FVector(DirectX::XMVector3Rotate(InVector.ToXMVector(), GetNormalized().ToXMVector()));
	}

	FVector UnrotateVector(const FVector& InVector) const noexcept
	{
		return FVector(DirectX::XMVector3InverseRotate(InVector.ToXMVector(), GetNormalized().ToXMVector()));
	}

	float GetAngle() const noexcept
	{
		const FQuat NormalizedQuat = GetNormalized();
		const float ClampedW = std::clamp(NormalizedQuat.W, -1.0f, 1.0f);
		return 2.0f * std::acos(ClampedW);
	}

	FVector GetRotationAxis(float Tolerance = KMath::Epsilon) const noexcept
	{
		const FQuat NormalizedQuat = GetNormalized();
		const float AxisSquared = NormalizedQuat.X * NormalizedQuat.X
			+ NormalizedQuat.Y * NormalizedQuat.Y
			+ NormalizedQuat.Z * NormalizedQuat.Z;
		if (AxisSquared <= Tolerance)
		{
			return FVector::ForwardVector;
		}
		const float InvAxisSize = 1.0f / std::sqrt(AxisSquared);
		return FVector(
			NormalizedQuat.X * InvAxisSize,
			NormalizedQuat.Y * InvAxisSize,
			NormalizedQuat.Z * InvAxisSize);
	}

	FVector GetAxisX() const noexcept { return RotateVector(FVector::ForwardVector); }
	FVector GetAxisY() const noexcept { return RotateVector(FVector::RightVector); }
	FVector GetAxisZ() const noexcept { return RotateVector(FVector::UpVector); }

	FVector GetForward() const noexcept { return GetAxisX(); }
	FVector GetRight()   const noexcept { return GetAxisY(); }
	FVector GetUp()      const noexcept { return GetAxisZ(); }

	float AngularDistance(const FQuat& Other) const noexcept
	{
		const float ClampedAbsDot = std::clamp(
			std::fabs(DotProduct(GetNormalized(), Other.GetNormalized())), -1.0f, 1.0f);
		return 2.0f * std::acos(ClampedAbsDot);
	}

	void EnforceShortestArcWith(const FQuat& Other) noexcept
	{
		if (DotProduct(*this, Other) < 0.0f)
		{
			X = -X; Y = -Y; Z = -Z; W = -W;
		}
	}

	// Defined after #include "Rotator.h" / "Matrix.h"
	FVector  Euler()    const noexcept;
	FRotator Rotator()  const noexcept;
	FMatrix  ToMatrix() const noexcept;

	// ──────────── static methods ────────────
public:
	static float DotProduct(const FQuat& A, const FQuat& B) noexcept
	{
		return DirectX::XMVectorGetX(DirectX::XMVector4Dot(A.ToXMVector(), B.ToXMVector()));
	}

	static FQuat Slerp(const FQuat& A, const FQuat& B, float Alpha) noexcept
	{
		FQuat AdjustedB = B;
		if (DotProduct(A, B) < 0.0f)
		{
			AdjustedB = -AdjustedB;
		}
		return FQuat(DirectX::XMQuaternionSlerp(A.GetNormalized().ToXMVector(), AdjustedB.GetNormalized().ToXMVector(), Alpha)).GetNormalized();
	}

	// Defined in Quat.cpp.
	static FQuat MakeFromEuler(const FVector& InEulerDegrees) noexcept;

	// ──────────── private helpers ────────────
private:
	static bool BuildOrthonormalBasisFromXY(
		const FVector& InX, const FVector& InY,
		FVector& OutX, FVector& OutY, FVector& OutZ) noexcept
	{
		OutX = InX.GetSafeNormal(KMath::Epsilon);
		if (OutX.IsNearlyZero(KMath::Epsilon)) return false;

		const FVector ProjectedY = InY - OutX * FVector::Dot(InY, OutX);
		OutY = ProjectedY.GetSafeNormal(KMath::Epsilon);
		if (OutY.IsNearlyZero(KMath::Epsilon)) return false;

		OutZ = FVector::Cross(OutX, OutY).GetSafeNormal(KMath::Epsilon);
		if (OutZ.IsNearlyZero(KMath::Epsilon)) return false;

		OutY = FVector::Cross(OutZ, OutX).GetSafeNormal(KMath::Epsilon);
		return !OutY.IsNearlyZero(KMath::Epsilon);
	}

	static bool BuildOrthonormalBasisFromXZ(
		const FVector& InX, const FVector& InZ,
		FVector& OutX, FVector& OutY, FVector& OutZ) noexcept
	{
		OutX = InX.GetSafeNormal(KMath::Epsilon);
		if (OutX.IsNearlyZero(KMath::Epsilon)) return false;

		const FVector ProjectedZ = InZ - OutX * FVector::Dot(InZ, OutX);
		OutZ = ProjectedZ.GetSafeNormal(KMath::Epsilon);
		if (OutZ.IsNearlyZero(KMath::Epsilon)) return false;

		OutY = FVector::Cross(OutZ, OutX).GetSafeNormal(KMath::Epsilon);
		if (OutY.IsNearlyZero(KMath::Epsilon)) return false;

		OutZ = FVector::Cross(OutX, OutY).GetSafeNormal(KMath::Epsilon);
		return !OutZ.IsNearlyZero(KMath::Epsilon);
	}

	static bool BuildOrthonormalBasisFromYZ(
		const FVector& InY, const FVector& InZ,
		FVector& OutX, FVector& OutY, FVector& OutZ) noexcept
	{
		OutY = InY.GetSafeNormal(KMath::Epsilon);
		if (OutY.IsNearlyZero(KMath::Epsilon)) return false;

		const FVector ProjectedZ = InZ - OutY * FVector::Dot(InZ, OutY);
		OutZ = ProjectedZ.GetSafeNormal(KMath::Epsilon);
		if (OutZ.IsNearlyZero(KMath::Epsilon)) return false;

		OutX = FVector::Cross(OutY, OutZ).GetSafeNormal(KMath::Epsilon);
		if (OutX.IsNearlyZero(KMath::Epsilon)) return false;

		OutZ = FVector::Cross(OutX, OutY).GetSafeNormal(KMath::Epsilon);
		return !OutZ.IsNearlyZero(KMath::Epsilon);
	}
};

inline constexpr FQuat FQuat::Identity { 0.f, 0.f, 0.f, 1.f };

inline FQuat operator*(float Scale, const FQuat& Q) noexcept { return Q * Scale; }
